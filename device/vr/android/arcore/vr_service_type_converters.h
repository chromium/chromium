// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_VR_SERVICE_TYPE_CONVERTERS_H_
#define DEVICE_VR_ANDROID_ARCORE_VR_SERVICE_TYPE_CONVERTERS_H_

#include "device/vr/android/arcore/arcore_sdk.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/transform.h"

namespace mojo {

template <>
struct TypeConverter<device::mojom::XRPlaneOrientation, ArPlaneType> {
  static device::mojom::XRPlaneOrientation Convert(ArPlaneType plane_type);
};

template <>
struct TypeConverter<gfx::Transform, device::mojom::VRPosePtr> {
  static gfx::Transform Convert(const device::mojom::VRPosePtr& pose);
};

template <>
struct TypeConverter<gfx::Transform, device::mojom::Pose> {
  static gfx::Transform Convert(const device::mojom::Pose& pose);
};

}  // namespace mojo

#endif  // DEVICE_VR_ANDROID_ARCORE_VR_SERVICE_TYPE_CONVERTERS_H_
