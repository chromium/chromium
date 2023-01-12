// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/username.h"

#include <vector>

#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // BUILDFLAG(IS_POSIX)

namespace remoting {

std::string GetUsername() {
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  long buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (buf_size <= 0) {
    return std::string();
  }

  std::vector<char> buf(buf_size);
  struct passwd passwd;
  struct passwd* passwd_result = nullptr;
  getpwuid_r(getuid(), &passwd, &(buf[0]), buf_size, &passwd_result);
  return passwd_result ? passwd_result->pw_name : std::string();
#else
  NOTIMPLEMENTED();
  return std::string();
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
}

}  // namespace remoting
