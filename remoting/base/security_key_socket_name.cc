// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/security_key_socket_name.h"

#include <stdlib.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "build/build_config.h"

namespace remoting {

namespace {
base::FilePath& GetSocketNameOverride() {
  static base::NoDestructor<base::FilePath> path;
  return *path;
}
}  // namespace

base::FilePath GetDefaultSecurityKeySocketName() {
  if (!GetSocketNameOverride().empty()) {
    return GetSocketNameOverride();
  }
#if BUILDFLAG(IS_LINUX)
  // LINT.IfChange(ssh_auth_sock_name)
  const char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (xdg_runtime_dir) {
    return base::FilePath(xdg_runtime_dir).Append("crd_ssh_auth_sock");
  }
  // LINT.ThenChange(//remoting/host/linux/linux_me2me_host.py:ssh_auth_sock_name)
  LOG(WARNING) << "Cannot find the XDG_RUNTIME_DIR environment variable.";
#else
  NOTIMPLEMENTED();
#endif
  return {};
}

void SetDefaultSecurityKeySocketNameForTest(const base::FilePath& path) {
  GetSocketNameOverride() = path;
}

}  // namespace remoting
