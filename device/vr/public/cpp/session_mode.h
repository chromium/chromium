// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_SESSION_MODE_H_
#define DEVICE_VR_PUBLIC_CPP_SESSION_MODE_H_

namespace device {

// This enum describes various modes of WebXR sessions that have been started.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SessionMode : int {
  kUnknown = 0,
  kInline = 1,
  kImmersiveVr = 2,
  kImmersiveAr = 3,
  kMaxValue = kImmersiveAr,
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_CPP_SESSION_MODE_H_
