// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/username.h"

#include <vector>

#include "base/notreached.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // defined(OS_POSIX)

namespace remoting {

std::string GetUsername() {
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  long buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (buf_size <= 0)
    return std::string();

  std::vector<char> buf(buf_size);
  struct passwd passwd;
  struct passwd* passwd_result = nullptr;
  getpwuid_r(getuid(), &passwd, &(buf[0]), buf_size, &passwd_result);
  return passwd_result ? passwd_result->pw_name : std::string();
#else
  NOTIMPLEMENTED();
  return std::string();
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID)
}

}  // namespace remoting
