// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_VR_SERVICE_MOJOM_TRAITS_H_
#define DEVICE_VR_PUBLIC_MOJOM_VR_SERVICE_MOJOM_TRAITS_H_

#include "device/vr/public/mojom/anchor_id.h"
#include "device/vr/public/mojom/hit_test_subscription_id.h"
#include "device/vr/public/mojom/layer_id.h"
#include "device/vr/public/mojom/plane_id.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/rgb_tuple_f32.h"
#include "device/vr/public/mojom/visibility_mask_id.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"

namespace mojo {

template <>
struct StructTraits<device::mojom::PlaneIdDataView, device::PlaneId> {
  static uint64_t id_value(const device::PlaneId& plane_id) {
    return plane_id.GetUnsafeValue();
  }

  static bool Read(device::mojom::PlaneIdDataView data, device::PlaneId* out) {
    *out = device::PlaneId(data.id_value());
    return true;
  }
};

template <>
struct StructTraits<device::mojom::AnchorIdDataView, device::AnchorId> {
  static uint64_t id_value(const device::AnchorId& anchor_id) {
    return anchor_id.GetUnsafeValue();
  }

  static bool Read(device::mojom::AnchorIdDataView data,
                   device::AnchorId* out) {
    *out = device::AnchorId(data.id_value());
    return true;
  }
};

template <>
struct StructTraits<device::mojom::XrVisibilityMaskIdDataView,
                    device::XrVisibilityMaskId> {
  static uint64_t id_value(const device::XrVisibilityMaskId& id) {
    return id.GetUnsafeValue();
  }

  static bool Read(device::mojom::XrVisibilityMaskIdDataView data,
                   device::XrVisibilityMaskId* out) {
    *out = device::XrVisibilityMaskId(data.id_value());
    return true;
  }
};

template <>
struct StructTraits<device::mojom::HitTestSubscriptionIdDataView,
                    device::HitTestSubscriptionId> {
  static uint64_t id_value(
      const device::HitTestSubscriptionId& hit_test_subscription_id) {
    return hit_test_subscription_id.GetUnsafeValue();
  }

  static bool Read(device::mojom::HitTestSubscriptionIdDataView data,
                   device::HitTestSubscriptionId* out) {
    *out = device::HitTestSubscriptionId(data.id_value());
    return true;
  }
};

template <>
struct StructTraits<device::mojom::LayerIdDataView, device::LayerId> {
  static uint64_t id_value(const device::LayerId& layer_id) {
    return layer_id.GetUnsafeValue();
  }

  static bool Read(device::mojom::LayerIdDataView data, device::LayerId* out) {
    *out = device::LayerId(data.id_value());
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
