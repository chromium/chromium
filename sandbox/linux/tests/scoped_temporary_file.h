// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_TESTS_SCOPED_TEMPORARY_FILE_H_
#define SANDBOX_LINUX_TESTS_SCOPED_TEMPORARY_FILE_H_

#include <string>

#include "build/build_config.h"

namespace sandbox {

#if BUILDFLAG(IS_ANDROID)
static const char kTempDirForTests[] = "/data/local/tmp/";
#else
static const char kTempDirForTests[] = "/tmp/";
#endif  // BUILDFLAG(IS_ANDROID)

// Creates and open a temporary file on creation and closes
// and removes it on destruction.
// Unlike base/ helpers, this does not require JNI on Android.
class ScopedTemporaryFile {
 public:
  ScopedTemporaryFile();

  ScopedTemporaryFile(const ScopedTemporaryFile&) = delete;
  ScopedTemporaryFile& operator=(const ScopedTemporaryFile&) = delete;

  ~ScopedTemporaryFile();

  int fd() const { return fd_; }
  const char* full_file_name() const { return full_file_name_.c_str(); }

 private:
  int fd_ = -1;
  std::string full_file_name_;
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_TESTS_SCOPED_TEMPORARY_FILE_H_
