// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_THREAD_POOL_UTIL_H_
#define CONTENT_COMMON_THREAD_POOL_UTIL_H_

#include <stddef.h>

namespace content {

// Returns the minimum number of foreground threads that the ThreadPool
// must have in a process that runs a renderer.
size_t GetMinForegroundThreadsInRendererThreadPool();

}  // namespace content

#endif  // CONTENT_COMMON_THREAD_POOL_UTIL_H_
