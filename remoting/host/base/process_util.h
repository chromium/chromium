// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_PROCESS_UTIL_H_
#define REMOTING_HOST_BASE_PROCESS_UTIL_H_

#include "base/files/file_path.h"
#include "base/process/process_handle.h"

namespace remoting {

// Gets the image path of |pid|. Note that on Linux, the process image's
// original path will still be returned even if the binary has been deleted from
// the storage.
base::FilePath GetProcessImagePath(base::ProcessId pid);

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_PROCESS_UTIL_H_
