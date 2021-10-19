// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_CPU_AFFINITY_SETTER_H_
#define CONTENT_COMMON_ANDROID_CPU_AFFINITY_SETTER_H_

#include "base/cpu_affinity_posix.h"

namespace content {

// Sets the given CPU affinity for the current thread and polls every 15 seconds
// to check if it was changed and if so sets it to |mode| again.
void SetCpuAffinityForCurrentThread(base::CpuAffinityMode mode);

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_CPU_AFFINITY_SETTER_H_
