// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_FILE_PATH_UTIL_H_
#define REMOTING_HOST_LINUX_FILE_PATH_UTIL_H_

#include <string>

#include "base/files/file_path.h"

namespace remoting {

// Returns the path to the directory that store host configurations.
base::FilePath GetConfigDirectoryPath();

// Returns a string that can be used to construct a host config file name, e.g.
// "host#1234567890aabbccddeeff1234567890".
std::string GetHostHash();

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_FILE_PATH_UTIL_H_
