// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_JOINT_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_JOINT_SPACE_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/xr/xr_hand.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRSession;

class XRJointSpace : public XRSpace {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRJointSpace(XRHand* hand,
               XRSession* session,
               std::unique_ptr<TransformationMatrix> mojo_from_joint,
               device::mojom::blink::XRHandJoint joint,
               float radius,
               device::mojom::XRHandedness handedness);

  float radius() const { return radius_; }
  device::mojom::blink::XRHandJoint joint() const { return joint_; }
  const String jointName() const;
  device::mojom::XRHandedness handedness() const { return handedness_; }

  absl::optional<TransformationMatrix> MojoFromNative() override;
  device::mojom::blink::XRNativeOriginInformationPtr NativeOrigin()
      const override;
  bool EmulatedPosition() const override;
  XRPose* getPose(XRSpace* other_space) override;

  void UpdateTracking(std::unique_ptr<TransformationMatrix> mojo_from_joint,
                      float radius);

  bool IsStationary() const override;

  std::string ToString() const override;

  bool handHasMissingPoses() const;

  void Trace(Visitor*) const override;

 private:
  Member<XRHand> hand_;
  std::unique_ptr<TransformationMatrix> mojo_from_joint_space_;
  const device::mojom::blink::XRHandJoint joint_;
  float radius_;
  const device::mojom::XRHandedness handedness_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_JOINT_SPACE_H_
