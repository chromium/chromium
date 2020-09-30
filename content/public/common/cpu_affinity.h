// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CPU_AFFINITY_H_
#define CONTENT_PUBLIC_COMMON_CPU_AFFINITY_H_

#include "base/cpu_affinity_posix.h"
#include "content/common/content_export.h"

namespace content {

// Enforce CPU affinity of the current process based on the CPU affinity mode.
// Use this to set up CPU-affinity restriction experiments (e.g. to restrict
// execution to little cores only). Should be called on the process's main
// thread during process startup after feature list initialization.
// The affinity might change at runtime (e.g. after Chrome goes back from
// background), so the content layer will set up a polling mechanism to enforce
// the given mode.
CONTENT_EXPORT void EnforceProcessCpuAffinity(base::CpuAffinityMode mode);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CPU_AFFINITY_H_
