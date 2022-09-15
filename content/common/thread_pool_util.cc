// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/thread_pool_util.h"

#include <algorithm>

#include "base/system/sys_info.h"

namespace content {

size_t GetMinForegroundThreadsInRendererThreadPool() {
  // Assume a busy main thread.
  return static_cast<size_t>(
      std::max(1, base::SysInfo::NumberOfProcessors() - 1));
}

}  // namespace content
