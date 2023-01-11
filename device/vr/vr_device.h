// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_H_
#define DEVICE_VR_VR_DEVICE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace device {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VrViewerType {
  GVR_UNKNOWN = 0,
  GVR_CARDBOARD = 1,
  GVR_DAYDREAM = 2,
  ORIENTATION_SENSOR_DEVICE = 10,
  FAKE_DEVICE = 11,
  // OPENVR_UNKNOWN = 20,
  // OPENVR_VIVE = 21,
  // OPENVR_RIFT_CV1 = 22,
  // OCULUS_UNKNOWN = 40,                 // Going through Oculus APIs
  // WINDOWS_MIXED_REALITY_UNKNOWN = 60,  // Going through WMR APIs
  OPENXR_UNKNOWN = 70,  // Going through OpenXR APIs
};

// Implemented in vr_device_base.cc
void COMPONENT_EXPORT(DEVICE_VR_BASE) LogViewerType(VrViewerType);

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_H_
