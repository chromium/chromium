// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include <stddef.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
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
  DCHECK(gpu_info);

  angle::SystemInfo system_info;
  bool success = angle::GetSystemInfo(&system_info);
  FillGPUInfoFromSystemInfo(gpu_info, &system_info);
  return success;
}

}  // namespace gpu
