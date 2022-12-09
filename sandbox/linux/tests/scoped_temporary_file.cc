// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/tests/scoped_temporary_file.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

namespace sandbox {

ScopedTemporaryFile::ScopedTemporaryFile() {
  static const char kFileNameTemplate[] = "ScopedTempFileXXXXXX";
  full_file_name_ = std::string(kTempDirForTests) + kFileNameTemplate;
  fd_ = mkstemp(full_file_name_.data());
  CHECK_LE(0, fd_);
}

ScopedTemporaryFile::~ScopedTemporaryFile() {
  CHECK_EQ(0, unlink(full_file_name_.c_str()));
  CHECK_EQ(0, IGNORE_EINTR(close(fd_)));
}

}  // namespace sandbox
