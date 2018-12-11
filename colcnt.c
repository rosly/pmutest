#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define L1_SIZE (32 * 1024) /* assuming 32KB L1 cache size */
#define COL_SIZE (L1_SIZE / sizeof(uint32_t)) /* single column will exced the L1 size */
#define ROW_SIZE 2048

//_Static_assert(L1_SIZE < (ROW_SIZE * sizeof(uint32_t)), "Single column data must overfill L1 cache");

static uint32_t (*matrix)[ROW_SIZE];
static uint32_t *result;

#ifndef FIXL1
void sumarize(void) {
	size_t x, y;
	uint32_t tmp;

	for (x = 0; x < ROW_SIZE; x++) {
		tmp = 0;
		for (y = 0; y < COL_SIZE; y++) 
			tmp += matrix[y][x];
		result[x] = tmp;
	}
}
#else
void sumarize(void) {
	size_t x, y;

	for (y = 0; y < COL_SIZE; y++) {
		for (x = 0; x < ROW_SIZE; x++) {
			result[x] += matrix[y][x];


#ifdef FIXPREF
			if (!(x % 16))
				__builtin_prefetch(&matrix[y+1][x], 0, 0);
#endif
		}
	}
}
#endif

void report(void) {
	size_t x;
	uint32_t res = 0;

	printf("\n");
	for (x = 0; x < ROW_SIZE; x++) {
		//printf("%"PRIu32" ", result[i]);
		res += result[x];
	}
	printf("%"PRIu32"\n",res);
	fflush(stdout);
}

int main(int argc, char *argv[]) {
	size_t bytes_total;
	size_t bytes_read;
	ssize_t ret;
	uint8_t *ptr;
	int fd;

	matrix = (uint32_t(*)[ROW_SIZE])calloc(COL_SIZE * ROW_SIZE, sizeof(uint32_t));
	result = (uint32_t*)calloc(ROW_SIZE, sizeof(uint32_t));
	if (!matrix || !result) {
		fprintf(stderr, "Cannot allocate memory for martix %zu bytes wide\n", sizeof(*matrix));
		return -1;
	}

	if (argc != 2) {
		fprintf(stderr, "Missing file arg. Call %s [file name]\n", argv[0]);
		return -1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("Cannot open input file");
		return -1;
	}

	bytes_total = ROW_SIZE * COL_SIZE * sizeof(uint32_t);
	printf("Reading %zu bytes ...\n", bytes_total);
	bytes_read = 0;
	while (bytes_read < bytes_total) {
		ptr = &((uint8_t*)matrix)[bytes_read];
		ret = read(fd, ptr, bytes_total - bytes_read);
		if (ret < 0) {
			perror("Error while reading matrix data");
			return -1;
		}
		if (0 == ret) {
			perror("Error while reading matrix data. File too small");
			return -1;
		}
		bytes_read += ret;
	}
	close(fd);

	sumarize();
	report();

	return 0;
}
