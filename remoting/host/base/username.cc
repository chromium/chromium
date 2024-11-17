// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/username.h"

#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // BUILDFLAG(IS_POSIX)

namespace remoting {

std::string GetUsername() {
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
  long buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (buf_size <= 0) {
    PLOG(ERROR) << "sysconf(_SC_GETPW_R_SIZE_MAX) failed";
    return std::string();
  }

  std::vector<char> buf(buf_size);
  struct passwd passwd;
  struct passwd* passwd_result = nullptr;
  uid_t user_id = getuid();
  getpwuid_r(user_id, &passwd, &(buf[0]), buf_size, &passwd_result);
  if (!passwd_result) {
    PLOG(ERROR) << "getpwuid_r() failed for user " << user_id;
    return std::string();
  }
  std::string result(passwd_result->pw_name);
  if (result.empty()) {
    LOG(ERROR) << "getpwuid_r() returned empty pw_name for user " << user_id;
  }
  return result;
#else
  NOTIMPLEMENTED();
  return std::string();
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
}

}  // namespace remoting
