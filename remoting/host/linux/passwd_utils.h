// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PASSWD_UTILS_H_
#define REMOTING_HOST_LINUX_PASSWD_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/strings/cstring_view.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"

namespace remoting {

struct PasswdUserInfo {
  std::string username;
  int uid;
  int gid;
  base::FilePath home_dir;
};

base::expected<PasswdUserInfo, Loggable> GetPasswdUserInfo(
    base::cstring_view username);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PASSWD_UTILS_H_
