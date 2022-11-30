// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include <stddef.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/notreached.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  // When command buffer is compiled as a standalone library, the process might
  // not have a Java environment.
  if (base::android::IsVMInitialized()) {
    gpu_info->machine_model_name =
        base::android::BuildInfo::GetInstance()->model();
  }

  // At this point GL bindings have been initialized already.
  return CollectGraphicsInfoGL(gpu_info, gl::GetDefaultDisplayEGL());
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  NOTREACHED();
  return false;
}

}  // namespace gpu
