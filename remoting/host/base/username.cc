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
  struct passwd* passwd_result;
  uid_t user_id = getuid();

  // Verify that `user_id` has a valid passwd database record, which matches
  // the UID for this process. It is possible for multiple usernames to share
  // the same user id, and some systems may not reliably support lookups by UID.
  // Therefore, use the username from the environment (which is set by the
  // parent process in remoting_user_session.cc), and do a lookup by name to ID.
  // The logic is similar to FindCurrentUsername() from
  // remoting_user_session.cc, except the re-entrant "_r" lookup functions are
  // used here, for safety in a multi-threaded program.
  for (const char* var : {"USER", "LOGNAME"}) {
    const char* username_from_env = getenv(var);
    if (!username_from_env) {
      LOG(WARNING) << "Name '" << var << "' not found in environment.";
      continue;
    }

    passwd_result = nullptr;
    errno = getpwnam_r(username_from_env, &passwd, &(buf[0]), buf_size,
                       &passwd_result);
    if (!passwd_result) {
      PLOG(WARNING) << "getpwnam_r() failed for username: "
                    << username_from_env;
      continue;
    }

    if (passwd_result->pw_uid != user_id) {
      LOG(WARNING) << "getpwnam_r() for user '" << username_from_env
                   << "' returned pw_uid " << passwd_result->pw_uid
                   << " which does not match user id " << user_id;
      continue;
    }

    return passwd_result->pw_name;
  }

  // The method above failed, fall back to a simple id->name lookup.
  passwd_result = nullptr;
  errno = getpwuid_r(user_id, &passwd, &(buf[0]), buf_size, &passwd_result);
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
