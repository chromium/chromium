// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following is the C version of code from base/process_utils_linux.cc.
// We shouldn't link against C++ code in a setuid binary.

// Needed for O_DIRECTORY, must be defined before fcntl.h is included
// (and it can be included earlier than the explicit #include below
// in some versions of glibc).
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "sandbox/linux/suid/process_util.h"

#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Ranges for the current (oom_score_adj) and previous (oom_adj)
// flavors of OOM score.
static const int kMaxOomScore = 1000;
static const int kMaxOldOomScore = 15;

// NOTE: This is not the only version of this function in the source:
// the base library (in process_util_linux.cc) also has its own C++ version.
bool AdjustOOMScore(pid_t process, int score) {
  if (score < 0 || score > kMaxOomScore)
    return false;

  char oom_adj[27];  // "/proc/" + log_10(2**64) + "\0"
                     //    6     +       20     +     1         = 27
  snprintf(oom_adj, sizeof(oom_adj), "/proc/%" PRIdMAX, (intmax_t)process);

  const int dirfd = open(oom_adj, O_RDONLY | O_DIRECTORY);
  if (dirfd < 0)
    return false;

  struct stat statbuf;
  if (fstat(dirfd, &statbuf) < 0) {
    close(dirfd);
    return false;
  }
  if (getuid() != statbuf.st_uid) {
    close(dirfd);
    return false;
  }

  int fd = openat(dirfd, "oom_score_adj", O_WRONLY);
  if (fd < 0) {
    // We failed to open oom_score_adj, so let's try for the older
    // oom_adj file instead.
    fd = openat(dirfd, "oom_adj", O_WRONLY);
    if (fd < 0) {
      // Nope, that doesn't work either.
      close(dirfd);
      return false;
    } else {
      // If we're using the old oom_adj file, the allowed range is now
      // [0, kMaxOldOomScore], so we scale the score.  This may result in some
      // aliasing of values, of course.
      score = score * kMaxOldOomScore / kMaxOomScore;
    }
  }
  close(dirfd);

  char buf[11];  // 0 <= |score| <= kMaxOomScore; using log_10(2**32) + 1 size
  snprintf(buf, sizeof(buf), "%d", score);
  size_t len = strlen(buf);

  ssize_t bytes_written = write(fd, buf, len);
  close(fd);
  return (bytes_written == (ssize_t)len);
}
