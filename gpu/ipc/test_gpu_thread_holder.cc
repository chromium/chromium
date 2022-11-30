// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/test_gpu_thread_holder.h"

#include "base/no_destructor.h"

namespace gpu {

InProcessGpuThreadHolder* GetTestGpuThreadHolder() {
  static base::NoDestructor<InProcessGpuThreadHolder> instance;
  return instance.get();
}

}  // namespace gpu
