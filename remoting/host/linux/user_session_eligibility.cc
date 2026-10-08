// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/user_session_eligibility.h"

#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"

namespace remoting {

namespace {

constexpr uid_t kRootUid = 0;
constexpr uid_t kMinRegularUserUid = 1000;
constexpr uid_t kNobodyUid = 65534;

}  // namespace

bool IsUidAllowedForDesktopSession(uid_t uid, bool is_greeter) {
  if (uid == kRootUid) {
    return false;
  }
  if (is_greeter) {
    return true;
  }
  return uid >= kMinRegularUserUid && uid != kNobodyUid;
}

bool IsValidLoginShell(const base::FilePath& shell,
                       base::span<const base::FilePath> valid_shells) {
  if (shell.empty()) {
    return false;
  }
  // Some systems list these in /etc/shells (e.g. for FTP-only accounts), but
  // they never provide an interactive login.
  const std::string basename = shell.BaseName().value();
  if (basename == "nologin" || basename == "false") {
    return false;
  }
  return std::ranges::contains(valid_shells, shell);
}

std::vector<base::FilePath> GetValidLoginShells() {
  std::vector<base::FilePath> shells;
  setusershell();
  while (const char* shell = getusershell()) {
    shells.emplace_back(shell);
  }
  endusershell();
  return shells;
}

}  // namespace remoting
