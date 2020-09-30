// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_CPU_AFFINITY_H_
#define CONTENT_COMMON_ANDROID_CPU_AFFINITY_H_

namespace content {

// Set up a regular polling to check that the current CPU affinity is set to
// the right mode and update it if it's not. The check is implemented as a
// TaskObserver that runs every 100th main thread task.
// This function should be called from the main thread during the initialization
// of the process. Subsequent calls from other threads will have no effect.
void SetupCpuAffinityPollingOnce();

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_CPU_AFFINITY_H_
