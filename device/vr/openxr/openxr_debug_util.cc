// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_debug_util.h"

#include "base/check.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace device {

namespace debug {
XrResult GetCurrentXrTime(const XrInstance& instance_,
                          const OpenXrExtensionHelper& extension_helper,
                          XrTime* current_time) {
  DCHECK(current_time);
#if BUILDFLAG(IS_WIN)
  LARGE_INTEGER system_now;
  QueryPerformanceCounter(&system_now);
  if (extension_helper.ExtensionMethods()
          .xrConvertWin32PerformanceCounterToTimeKHR == nullptr) {
    return XR_ERROR_FUNCTION_UNSUPPORTED;
  }

  XrResult result = extension_helper.ExtensionMethods()
                        .xrConvertWin32PerformanceCounterToTimeKHR(
                            instance_, &system_now, current_time);
  if (XR_FAILED(result)) {
    DLOG(ERROR) << __func__ << " Failed with: " << result;
    // We don't clear the current_time state as we assume that the OpenXr method
    // leaves it in an okay state.
  }

  return result;
#else
  return XR_ERROR_FUNCTION_UNSUPPORTED;
#endif
}
}  // namespace debug

}  // namespace device
