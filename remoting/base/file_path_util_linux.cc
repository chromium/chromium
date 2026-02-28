// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/file_path_util_linux.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/obsolete/md5.h"
#include "net/base/network_interfaces.h"

namespace remoting {
namespace {
const base::FilePath::CharType kConfigDir[] =
    FILE_PATH_LITERAL("chrome-remote-desktop");
}  // namespace

std::string GetHostHash() {
  return "host#" +
         base::HexEncodeLower(crypto::obsolete::Md5::Hash(net::GetHostName()));
}

base::FilePath GetMultiProcessHostGlobalConfigDir() {
  return base::FilePath("/etc").Append(kConfigDir);
}

base::FilePath GetPerUserConfigDir() {
  return base::GetHomeDir().Append(".config").Append(kConfigDir);
}

}  // namespace remoting
