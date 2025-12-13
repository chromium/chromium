// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/file_path_util_linux.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/obsolete/md5.h"
#include "net/base/network_interfaces.h"

namespace remoting {

base::FilePath GetConfigDirectoryPath() {
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  return homedir.Append(".config/chrome-remote-desktop");
}

std::string GetHostHash() {
  return "host#" +
         base::HexEncodeLower(crypto::obsolete::Md5::Hash(net::GetHostName()));
}

}  // namespace remoting
