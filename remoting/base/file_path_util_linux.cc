// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/file_path_util_linux.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"

namespace remoting {
namespace {
const base::FilePath::CharType kChromeRemoteDesktopDir[] =
    FILE_PATH_LITERAL("chrome-remote-desktop");
}  // namespace

base::FilePath GetVarLibDir() {
  return base::FilePath("/var/lib").Append(kChromeRemoteDesktopDir);
}

base::FilePath GetMultiProcessHostGlobalConfigDir() {
  return base::FilePath("/etc").Append(kChromeRemoteDesktopDir);
}

base::FilePath GetPerUserConfigDir() {
  return base::GetHomeDir().Append(GetPerUserConfigRelativeDir());
}

base::FilePath GetPerUserConfigRelativeDir() {
  return base::FilePath(".config").Append(kChromeRemoteDesktopDir);
}

}  // namespace remoting
