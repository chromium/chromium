// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <getopt.h>
#include <limits.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "media/gpu/macros.h"

#define FOURCC_SIZE 4

enum logging_levels {
  kLoggingInfo = 0,
  kLoggingError,
  kLoggingFatal,
  kLoggingLevelMax
};

#define DEFAULT_LOG_LEVEL kLoggingInfo

#define LOGGING(level, stream, fmt, ...)   \
  do {                                     \
    if (level >= log_run_level) {          \
      fprintf(stream, fmt, ##__VA_ARGS__); \
      fprintf(stream, "\n");               \
    }                                      \
  } while (0)

#define LOG_INFO(fmt, ...) LOGGING(kLoggingInfo, stdout, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOGGING(kLoggingError, stderr, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)                             \
  do {                                                  \
    LOGGING(kLoggingFatal, stderr, fmt, ##__VA_ARGS__); \
    exit(EXIT_FAILURE);                                 \
  } while (0)

static const char* kDecodeDevice = "/dev/video-dec0";
static const uint32_t kIVFHeaderSignature = v4l2_fourcc('D', 'K', 'I', 'F');

static int log_run_level = DEFAULT_LOG_LEVEL;

struct ivf_file_header {
  uint32_t signature;
  uint16_t version;
  uint16_t header_length;
  uint32_t fourcc;
  uint16_t width;
  uint16_t height;
  uint32_t denominator;
  uint32_t numerator;
  uint32_t frame_cnt;
  uint32_t unused;
} __attribute__((packed));

struct compressed_file {
  FILE* fp;
  struct ivf_file_header header;
};

void fourcc_to_string(uint32_t fourcc, char* fourcc_string) {
  sprintf(fourcc_string, "%c%c%c%c", fourcc & 0xff, fourcc >> 8 & 0xff,
          fourcc >> 16 & 0xff, fourcc >> 24 & 0xff);
}

// For stateless API, fourcc |VP9F| is needed instead of |VP90| for VP9 codec.
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/pixfmt-compressed.html
uint32_t file_fourcc_to_driver_fourcc(uint32_t header_fourcc) {
  if (header_fourcc == V4L2_PIX_FMT_VP9) {
    LOG_INFO("OUTPUT format mapped from VP90 to VP9F.");
    return V4L2_PIX_FMT_VP9_FRAME;
  }

  return header_fourcc;
}

struct compressed_file open_file(const char* file_name) {
  struct compressed_file file = {0};

  FILE* fp = fopen(file_name, "rb");
  if (fp) {
    if (fread(&file.header, sizeof(struct ivf_file_header), 1, fp) != 1) {
      fclose(fp);
      LOG_ERROR("Unable to read ivf file header.");
    }

    if (file.header.signature != kIVFHeaderSignature) {
      fclose(fp);
      LOG_ERROR("Incorrect header signature : 0x%0x != 0x%0x",
                file.header.signature, kIVFHeaderSignature);
    }

    file.fp = fp;

    char fourcc[FOURCC_SIZE + 1];
    fourcc_to_string(file.header.fourcc, fourcc);
    LOG_INFO("OUTPUT format: %s", fourcc);

    LOG_INFO("Ivf file header: %d x %d", file.header.width, file.header.height);
    // |width| and |height| should be even numbers.
    assert((file.header.width % 2) == 0);
    assert((file.header.height % 2) == 0);
    LOG_INFO("Ivf file header: frame_cnt = %d", file.header.frame_cnt);
  } else {
    LOG_ERROR("Unable to open file: %s.", file_name);
  }

  return file;
}

int query_format(int v4lfd, enum v4l2_buf_type type, uint32_t fourcc) {
  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));

  fmtdesc.type = type;
  while (ioctl(v4lfd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (fourcc == 0) {
      char fourcc_str[FOURCC_SIZE + 1];
      fourcc_to_string(fmtdesc.pixelformat, fourcc_str);
      LOG_INFO("%s", fourcc_str);
    } else if (fourcc == fmtdesc.pixelformat)
      return 1;
    fmtdesc.index++;
  }

  return 0;
}

int capabilities(int v4lfd,
                 uint32_t compressed_format,
                 uint32_t uncompressed_format) {
  struct v4l2_capability cap;
  memset(&cap, 0, sizeof(cap));
  int ret = ioctl(v4lfd, VIDIOC_QUERYCAP, &cap);
  if (ret != 0)
    LOG_ERROR("VIDIOC_QUERYCAP failed: %s.", strerror(errno));

  LOG_INFO("Driver=\"%s\" bus_info=\"%s\" card=\"%s\" fd=0x%x", cap.driver,
           cap.bus_info, cap.card, v4lfd);

  if (!query_format(v4lfd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                    compressed_format)) {
    LOG_ERROR("Supported compressed formats:");
    query_format(v4lfd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 0);
    ret = 1;
  }

  if (!query_format(v4lfd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                    uncompressed_format)) {
    LOG_ERROR("Supported uncompressed formats:");
    query_format(v4lfd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 0);
    ret = 1;
  }

  return ret;
}

static void print_help(const char* argv0) {
  printf("usage: %s [OPTIONS]\n", argv0);
  printf("  -f, --file        ivf file to decode\n");
  printf("  -m, --max         max number of visible frames to decode\n");
  printf("  -o, --output_fmt  fourcc of output format\n");
  printf("  -l, --log_level   specifies log level, 0:info 1:error 2:fatal \n");
}

static const struct option longopts[] = {
    {"file", required_argument, NULL, 'f'},
    {"max", required_argument, NULL, 'm'},
    {"output_fmt", no_argument, NULL, 'o'},
    {"log_level", required_argument, NULL, 'l'},
    {0, 0, 0, 0},
};

int main(int argc, char* argv[]) {
  int c;
  char* file_name = NULL;
  bool print_md5hash = false;
  uint32_t frames_to_decode = UINT_MAX;
  uint32_t uncompressed_fourcc = v4l2_fourcc('M', 'M', '2', '1');
  // TODO(stevecho): handle V4L2_MEMORY_DMABUF case
  uint32_t CAPTURE_memory = V4L2_MEMORY_MMAP;

  while ((c = getopt_long(argc, argv, "m:f:o:l:", longopts, NULL)) != -1) {
    switch (c) {
      case 'f':
        file_name = strdup(optarg);
        break;
      case 'm':
        frames_to_decode = atoi(optarg);
        break;
      case 'o':
        if (strlen(optarg) == 4) {
          uncompressed_fourcc =
              v4l2_fourcc(toupper(optarg[0]), toupper(optarg[1]),
                          toupper(optarg[2]), toupper(optarg[3]));
          // TODO(stevecho): support modifier using minigbm
        }
        break;
      case 'l': {
        const uint32_t specified_log_run_level = atoi(optarg);
        if (specified_log_run_level >= kLoggingLevelMax) {
          LOG_INFO("Undefined log level %d, using default log level instead.",
                   specified_log_run_level);
        } else {
          log_run_level = specified_log_run_level;
        }
        break;
      }
      default:
        break;
    }
  }

  LOG_INFO("V4L2 stateless decoder.");

  if (frames_to_decode != UINT_MAX)
    LOG_INFO("Only decoding a max of %d frames.", frames_to_decode);

  char fourcc[FOURCC_SIZE + 1];
  fourcc_to_string(uncompressed_fourcc, fourcc);
  LOG_INFO("CAPTURE format: %s", fourcc);

  if (!file_name) {
    print_help(argv[0]);
    exit(1);
  }

  struct compressed_file compressed_file = open_file(file_name);
  if (!compressed_file.fp)
    LOG_FATAL("Unable to open ivf file: %s.", file_name);

  int v4lfd = open(kDecodeDevice, O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (v4lfd < 0)
    LOG_FATAL("Unable to open device file: %s.", kDecodeDevice);

  const uint32_t driver_fourcc =
      file_fourcc_to_driver_fourcc(compressed_file.header.fourcc);

  if (capabilities(v4lfd, driver_fourcc, uncompressed_fourcc) != 0) {
    LOG_FATAL("Capabilities not present for decode.");
  }

  close(v4lfd);
  fclose(compressed_file.fp);
  free(file_name);

  return 0;
}
