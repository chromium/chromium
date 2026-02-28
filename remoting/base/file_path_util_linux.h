// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_FILE_PATH_UTIL_LINUX_H_
#define REMOTING_BASE_FILE_PATH_UTIL_LINUX_H_

#include <string>

#include "base/files/file_path.h"

namespace remoting {

// Returns a string that can be used to construct a host config file name, e.g.
// "host#1234567890aabbccddeeff1234567890".
// DEPRECATED: This should only be used for the single-process host for
// compatibility reasons. New config/setting files should not have the host hash
// in the filename.
std::string GetHostHash();

// Returns the directory where the host config file for the multi-process host
// is located. Note that only processes run as root will have access to files in
// the directory.
base::FilePath GetMultiProcessHostGlobalConfigDir();

// Returns the per-user chromoting config directory.
// On the single-process host, this is where the host config file is located,
// i.e. this is what `GetConfigDir()` returns.
// On the multi-process host, this will return a path in the home directory of
// the user that the process is run as, so this is generally only useful for the
// desktop process, which is always run as the login user.
base::FilePath GetPerUserConfigDir();

}  // namespace remoting

#endif  // REMOTING_BASE_FILE_PATH_UTIL_LINUX_H_
