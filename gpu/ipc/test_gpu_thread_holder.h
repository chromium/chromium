// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_TEST_GPU_THREAD_HOLDER_H_
#define GPU_IPC_TEST_GPU_THREAD_HOLDER_H_

#include "base/component_export.h"
#include "gpu/ipc/in_process_gpu_thread_holder.h"

namespace gpu {

// Returns a global InProcessGpuThreadHolder instance that can be used to get a
// task executor for use with InProcessCommandBuffer in tests. Any changes to
// GpuPreferences or GpuFeatureInfo should be done during test suite
// initialization before *any* tests run.
COMPONENT_EXPORT(GPU_THREAD_HOLDER)
InProcessGpuThreadHolder* GetTestGpuThreadHolder();

}  // namespace gpu

#endif  // GPU_IPC_TEST_GPU_THREAD_HOLDER_H_
