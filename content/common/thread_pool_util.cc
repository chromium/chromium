// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/thread_pool_util.h"

#include <algorithm>

#include "base/system/sys_info.h"

namespace content {

int GetMinForegroundThreadsInRendererThreadPool() {
  // Assume a busy main thread.
  return std::max(1, base::SysInfo::NumberOfProcessors() - 1);
}

}  // namespace content
