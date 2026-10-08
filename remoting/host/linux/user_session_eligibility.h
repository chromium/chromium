// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_USER_SESSION_ELIGIBILITY_H_
#define REMOTING_HOST_LINUX_USER_SESSION_ELIGIBILITY_H_

#include <sys/types.h>

#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"

namespace remoting {

// Returns whether a desktop session may be run as `uid`. Root (UID 0) is
// always rejected. System accounts (UID < 1000) and `nobody` (UID 65534) are
// rejected unless `is_greeter` is true.
bool IsUidAllowedForDesktopSession(uid_t uid, bool is_greeter);

// Returns whether `shell` is one of `valid_shells`, which is typically the
// result of GetValidLoginShells(). An empty `shell` is never valid, nor is a
// shell named `nologin` or `false`, even if it is listed in `valid_shells`.
bool IsValidLoginShell(const base::FilePath& shell,
                       base::span<const base::FilePath> valid_shells);

// Returns the valid login shells listed in /etc/shells, as reported by
// getusershell(3). If /etc/shells can't be read, glibc returns a default list
// (`/bin/sh` and `/bin/csh`) instead. This may block, and must not be called
// concurrently with other users of getusershell().
std::vector<base::FilePath> GetValidLoginShells();

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_USER_SESSION_ELIGIBILITY_H_
