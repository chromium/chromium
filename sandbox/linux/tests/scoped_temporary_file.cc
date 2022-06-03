// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/tests/scoped_temporary_file.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

namespace sandbox {

ScopedTemporaryFile::ScopedTemporaryFile() : fd_(-1) {
#if defined(OS_ANDROID)
  static const char file_template[] = "/data/local/tmp/ScopedTempFileXXXXXX";
#else
  static const char file_template[] = "/tmp/ScopedTempFileXXXXXX";
#endif  // defined(OS_ANDROID)
  static_assert(sizeof(full_file_name_) >= sizeof(file_template),
                "full_file_name is not large enough");
  memcpy(full_file_name_, file_template, sizeof(file_template));
  fd_ = mkstemp(full_file_name_);
  CHECK_LE(0, fd_);
}

ScopedTemporaryFile::~ScopedTemporaryFile() {
  CHECK_EQ(0, unlink(full_file_name_));
  CHECK_EQ(0, IGNORE_EINTR(close(fd_)));
}

}  // namespace sandbox
