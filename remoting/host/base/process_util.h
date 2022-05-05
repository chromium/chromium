// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_PROCESS_UTIL_H_
#define REMOTING_HOST_BASE_PROCESS_UTIL_H_

#include "base/files/file_path.h"
#include "base/process/process_handle.h"

namespace remoting {

// Gets the image path of |pid|.
base::FilePath GetProcessImagePath(base::ProcessId pid);

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_PROCESS_UTIL_H_
