// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_EXIT_XR_PRESENT_REASON_H_
#define DEVICE_VR_OPENXR_EXIT_XR_PRESENT_REASON_H_

enum class ExitXrPresentReason : int32_t {
  kUnknown = 0,
  kMojoConnectionError = 1,
  kOpenXrUninitialize = 2,
  kStartRuntimeFailed = 3,
  kOpenXrStartFailed = 4,
  kXrEndFrameFailed = 5,
  kGetFrameAfterSessionEnded = 6,
  kSubmitFrameFailed = 7,
  kBrowserShutdown = 8,
  kXrPlatformHelperShutdown = 9,
};

#endif  // DEVICE_VR_OPENXR_EXIT_XR_PRESENT_REASON_H_
