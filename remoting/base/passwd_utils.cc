// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/passwd_utils.h"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "remoting/base/loggable.h"

namespace remoting {

base::expected<PasswdUserInfo, Loggable> GetPasswdUserInfo(
    base::cstring_view username) {
  struct passwd pw;
  struct passwd* result;
  constexpr int kDefaultPwnameLength = 1024;
  long user_name_length = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (user_name_length == -1) {
    user_name_length = kDefaultPwnameLength;
  }
  std::vector<char> buffer(user_name_length);
  int err =
      getpwnam_r(username.c_str(), &pw, buffer.data(), buffer.size(), &result);
  if (err != 0) {
    return base::unexpected(
        Loggable(FROM_HERE, base::StringPrintf("getpwnam_r failed: %s (%d)",
                                               strerror(errno), errno)));
  }
  if (result == nullptr) {
    return base::unexpected(Loggable(
        FROM_HERE, base::StringPrintf("User not found: %s", username.data())));
  }
  PasswdUserInfo user_info;
  user_info.username = result->pw_name;
  user_info.uid = result->pw_uid;
  user_info.gid = result->pw_gid;
  user_info.home_dir = base::FilePath(result->pw_dir);

#if BUILDFLAG(IS_LINUX)
  long max_groups = sysconf(_SC_NGROUPS_MAX);
  constexpr int kDefaultNgroups = 64;
  int ngroups =
      (max_groups > 0) ? static_cast<int>(max_groups) : kDefaultNgroups;
  std::vector<gid_t> groups(ngroups);
  if (getgrouplist(result->pw_name, result->pw_gid, groups.data(), &ngroups) ==
      -1) {
    if (ngroups > 0) {
      groups.resize(ngroups);
      if (getgrouplist(result->pw_name, result->pw_gid, groups.data(),
                       &ngroups) == -1) {
        return base::unexpected(Loggable(
            FROM_HERE, base::StringPrintf("getgrouplist failed: %s (%d)",
                                          strerror(errno), errno)));
      }
    }
  }
  if (ngroups >= 0) {
    groups.resize(ngroups);
  }
  user_info.supplementary_gids = std::move(groups);
#endif
  return user_info;
}

}  // namespace remoting
