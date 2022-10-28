// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_DEBUG_UTIL_H_
#define DEVICE_VR_OPENXR_OPENXR_DEBUG_UTIL_H_

#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

// We want to discourage general usage of these methods without being
// intentional about it. We use a debug namespace to help ensure they aren't
// accidentally used.
namespace debug {
// XrTime is generally meant to be an opaque concept; and we should thus think
// carefully about using it. If we want to leverage this more broadly than for
// debugging, we should think carefully about how to construct/get it so
// that we can ensure we're using it in appropriate ways. Of particular
// concern is that there may be time drift between XrTime and the system time,
// so the value returned from this shouldn't be stored and used as the basis
// of many/any calculations.
XrResult GetCurrentXrTime(const XrInstance& instance_,
                          const OpenXrExtensionHelper& extension_helper,
                          XrTime* current_time);
}  // namespace debug

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_DEBUG_UTIL_H_
