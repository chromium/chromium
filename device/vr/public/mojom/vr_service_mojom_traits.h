// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_VR_SERVICE_MOJOM_TRAITS_H_
#define DEVICE_VR_PUBLIC_MOJOM_VR_SERVICE_MOJOM_TRAITS_H_

#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/rgb_tuple_f32.h"
#include "device/vr/public/mojom/rgba_tuple_f16.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"

namespace mojo {

template <>
struct StructTraits<device::mojom::RgbaTupleF16DataView, device::RgbaTupleF16> {
  static uint16_t red(const device::RgbaTupleF16& rgba) { return rgba.red(); }
  static uint16_t green(const device::RgbaTupleF16& rgba) {
    return rgba.green();
  }
  static uint16_t blue(const device::RgbaTupleF16& rgba) { return rgba.blue(); }
  static uint16_t alpha(const device::RgbaTupleF16& rgba) {
    return rgba.alpha();
  }
  static bool Read(device::mojom::RgbaTupleF16DataView data,
                   device::RgbaTupleF16* out) {
    out->set_red(data.red());
    out->set_green(data.green());
    out->set_blue(data.blue());
    out->set_alpha(data.alpha());
    return true;
  }
};

template <>
struct StructTraits<device::mojom::RgbTupleF32DataView, device::RgbTupleF32> {
  static float red(const device::RgbTupleF32& rgba) { return rgba.red(); }
  static float green(const device::RgbTupleF32& rgba) { return rgba.green(); }
  static float blue(const device::RgbTupleF32& rgba) { return rgba.blue(); }
  static bool Read(device::mojom::RgbTupleF32DataView data,
                   device::RgbTupleF32* out) {
    out->set_red(data.red());
    out->set_green(data.green());
    out->set_blue(data.blue());
    return true;
  }
};

template <>
class StructTraits<device::mojom::PoseDataView, device::Pose> {
 public:
  static const gfx::Point3F& position(const device::Pose& pose) {
    return pose.position();
  }
  static const gfx::Quaternion& orientation(const device::Pose& pose) {
    return pose.orientation();
  }

  static bool Read(device::mojom::PoseDataView pose_data,
                   device::Pose* out_pose) {
    gfx::Point3F position;
    if (!pose_data.ReadPosition(&position)) {
      return false;
    }

    gfx::Quaternion orientation;
    if (!pose_data.ReadOrientation(&orientation)) {
      return false;
    }

    *out_pose = device::Pose(position, orientation);
    return true;
  }
};

}  // namespace mojo

#endif  // DEVICE_VR_PUBLIC_MOJOM_VR_SERVICE_MOJOM_TRAITS_H_
