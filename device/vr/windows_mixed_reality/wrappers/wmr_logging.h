// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_LOGGING_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_LOGGING_H_

#include "base/macros.h"
#include "base/win/windows_types.h"

namespace device {
// This enum is used in TRACE_EVENTs.  Try to keep enum values the same to make
// analysis easier across builds.
enum class WMRErrorLocation {
  kAcquireCurrentStage = 1,
  kStationaryReferenceCreation = 2,
  kGetTransformBetweenOrigins = 3,
  kGamepadMissingTimestamp = 4,
  kGamepadMissingOrigin = 5,
  kAttachedReferenceCreation = 6,
  kGetSpatialLocator = 7,
};

// These methods(and the above enum(s)) are designed to facilitate more verbose
// logging from within the windows_mixed_reality classes, without adding
// additional string bloat to the binary size, by allowing us to log out
// generic error or info events with an enum as a "line number" or "identifier".
namespace WMRLogging {
void TraceError(WMRErrorLocation location);
void TraceError(WMRErrorLocation location, HRESULT hr);
}  // namespace WMRLogging

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_LOGGING_H_
