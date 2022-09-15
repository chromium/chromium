// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_IN_PROCESS_GPU_THREAD_H_
#define CONTENT_GPU_IN_PROCESS_GPU_THREAD_H_

#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/in_process_child_thread_params.h"
#include "gpu/config/gpu_preferences.h"

namespace content {

class ChildProcess;

// This class creates a GPU thread (instead of a GPU process), when running
// with --in-process-gpu or --single-process.
class InProcessGpuThread : public base::Thread {
 public:
  explicit InProcessGpuThread(const InProcessChildThreadParams& params,
                              const gpu::GpuPreferences& gpu_preferences);

  InProcessGpuThread(const InProcessGpuThread&) = delete;
  InProcessGpuThread& operator=(const InProcessGpuThread&) = delete;

  ~InProcessGpuThread() override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  InProcessChildThreadParams params_;

  // Deleted in CleanUp() on the gpu thread, so don't use smart pointers.
  std::unique_ptr<ChildProcess> gpu_process_;
  gpu::GpuPreferences gpu_preferences_;
};

CONTENT_EXPORT base::Thread* CreateInProcessGpuThread(
    const InProcessChildThreadParams& params,
    const gpu::GpuPreferences& gpu_preferences);

}  // namespace content

#endif  // CONTENT_GPU_IN_PROCESS_GPU_THREAD_H_
