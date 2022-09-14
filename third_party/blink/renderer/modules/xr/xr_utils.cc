// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_utils.h"

#include <cmath>

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

DOMFloat32Array* transformationMatrixToDOMFloat32Array(
    const TransformationMatrix& matrix) {
  float array[16];
  matrix.GetColMajorF(array);
  return DOMFloat32Array::Create(array, 16);
}

TransformationMatrix DOMFloat32ArrayToTransformationMatrix(DOMFloat32Array* m) {
  DCHECK_EQ(m->length(), 16u);
  return TransformationMatrix::ColMajorF(m->Data());
}

TransformationMatrix WTFFloatVectorToTransformationMatrix(
    const Vector<float>& m) {
  DCHECK_EQ(m.size(), 16u);
  return TransformationMatrix::ColMajorF(m.data());
}

// Normalize to have length = 1.0
DOMPointReadOnly* makeNormalizedQuaternion(double x,
                                           double y,
                                           double z,
                                           double w) {
  double length = std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
  if (length == 0.0) {
    // Return a default value instead of crashing.
    return DOMPointReadOnly::Create(0.0, 0.0, 0.0, 1.0);
  }
  return DOMPointReadOnly::Create(x / length, y / length, z / length,
                                  w / length);
}

WebGLRenderingContextBase* webglRenderingContextBaseFromUnion(
    const V8XRWebGLRenderingContext* context) {
  DCHECK(context);
  switch (context->GetContentType()) {
    case V8XRWebGLRenderingContext::ContentType::kWebGL2RenderingContext:
      return context->GetAsWebGL2RenderingContext();
    case V8XRWebGLRenderingContext::ContentType::kWebGLRenderingContext:
      return context->GetAsWebGLRenderingContext();
  }
  NOTREACHED();
  return nullptr;
}

absl::optional<device::Pose> CreatePose(
    const blink::TransformationMatrix& matrix) {
  return device::Pose::Create(matrix.ToTransform());
}

device::mojom::blink::XRHandJoint StringToMojomHandJoint(
    const String& hand_joint_string) {
  if (hand_joint_string == "wrist") {
    return device::mojom::blink::XRHandJoint::kWrist;
  } else if (hand_joint_string == "thumb-metacarpal") {
    return device::mojom::blink::XRHandJoint::kThumbMetacarpal;
  } else if (hand_joint_string == "thumb-phalanx-proximal") {
    return device::mojom::blink::XRHandJoint::kThumbPhalanxProximal;
  } else if (hand_joint_string == "thumb-phalanx-distal") {
    return device::mojom::blink::XRHandJoint::kThumbPhalanxDistal;
  } else if (hand_joint_string == "thumb-tip") {
    return device::mojom::blink::XRHandJoint::kThumbTip;
  } else if (hand_joint_string == "index-finger-metacarpal") {
    return device::mojom::blink::XRHandJoint::kIndexFingerMetacarpal;
  } else if (hand_joint_string == "index-finger-phalanx-proximal") {
    return device::mojom::blink::XRHandJoint::kIndexFingerPhalanxProximal;
  } else if (hand_joint_string == "index-finger-phalanx-intermediate") {
    return device::mojom::blink::XRHandJoint::kIndexFingerPhalanxIntermediate;
  } else if (hand_joint_string == "index-finger-phalanx-distal") {
    return device::mojom::blink::XRHandJoint::kIndexFingerPhalanxDistal;
  } else if (hand_joint_string == "index-finger-tip") {
    return device::mojom::blink::XRHandJoint::kIndexFingerTip;
  } else if (hand_joint_string == "middle-finger-metacarpal") {
    return device::mojom::blink::XRHandJoint::kMiddleFingerMetacarpal;
  } else if (hand_joint_string == "middle-finger-phalanx-proximal") {
    return device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxProximal;
  } else if (hand_joint_string == "middle-finger-phalanx-intermediate") {
    return device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxIntermediate;
  } else if (hand_joint_string == "middle-finger-phalanx-distal") {
    return device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxDistal;
  } else if (hand_joint_string == "middle-finger-tip") {
    return device::mojom::blink::XRHandJoint::kMiddleFingerTip;
  } else if (hand_joint_string == "ring-finger-metacarpal") {
    return device::mojom::blink::XRHandJoint::kRingFingerMetacarpal;
  } else if (hand_joint_string == "ring-finger-phalanx-proximal") {
    return device::mojom::blink::XRHandJoint::kRingFingerPhalanxProximal;
  } else if (hand_joint_string == "ring-finger-phalanx-intermediate") {
    return device::mojom::blink::XRHandJoint::kRingFingerPhalanxIntermediate;
  } else if (hand_joint_string == "ring-finger-phalanx-distal") {
    return device::mojom::blink::XRHandJoint::kRingFingerPhalanxDistal;
  } else if (hand_joint_string == "ring-finger-tip") {
    return device::mojom::blink::XRHandJoint::kRingFingerTip;
  } else if (hand_joint_string == "pinky-finger-metacarpal") {
    return device::mojom::blink::XRHandJoint::kPinkyFingerMetacarpal;
  } else if (hand_joint_string == "pinky-finger-phalanx-proximal") {
    return device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxProximal;
  } else if (hand_joint_string == "pinky-finger-phalanx-intermediate") {
    return device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxIntermediate;
  } else if (hand_joint_string == "pinky-finger-phalanx-distal") {
    return device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxDistal;
  } else if (hand_joint_string == "pinky-finger-tip") {
    return device::mojom::blink::XRHandJoint::kPinkyFingerTip;
  }

  NOTREACHED();
  return device::mojom::blink::XRHandJoint::kMaxValue;
}

String MojomHandJointToString(device::mojom::blink::XRHandJoint hand_joint) {
  switch (hand_joint) {
    case device::mojom::blink::XRHandJoint::kWrist:
      return "wrist";
    case device::mojom::blink::XRHandJoint::kThumbMetacarpal:
      return "thumb-metacarpal";
    case device::mojom::blink::XRHandJoint::kThumbPhalanxProximal:
      return "thumb-phalanx-proximal";
    case device::mojom::blink::XRHandJoint::kThumbPhalanxDistal:
      return "thumb-phalanx-distal";
    case device::mojom::blink::XRHandJoint::kThumbTip:
      return "thumb-tip";
    case device::mojom::blink::XRHandJoint::kIndexFingerMetacarpal:
      return "index-finger-metacarpal";
    case device::mojom::blink::XRHandJoint::kIndexFingerPhalanxProximal:
      return "index-finger-phalanx-proximal";
    case device::mojom::blink::XRHandJoint::kIndexFingerPhalanxIntermediate:
      return "index-finger-phalanx-intermediate";
    case device::mojom::blink::XRHandJoint::kIndexFingerPhalanxDistal:
      return "index-finger-phalanx-distal";
    case device::mojom::blink::XRHandJoint::kIndexFingerTip:
      return "index-finger-tip";
    case device::mojom::blink::XRHandJoint::kMiddleFingerMetacarpal:
      return "middle-finger-metacarpal";
    case device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxProximal:
      return "middle-finger-phalanx-proximal";
    case device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxIntermediate:
      return "middle-finger-phalanx-intermediate";
    case device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxDistal:
      return "middle-finger-phalanx-distal";
    case device::mojom::blink::XRHandJoint::kMiddleFingerTip:
      return "middle-finger-tip";
    case device::mojom::blink::XRHandJoint::kRingFingerMetacarpal:
      return "ring-finger-metacarpal";
    case device::mojom::blink::XRHandJoint::kRingFingerPhalanxProximal:
      return "ring-finger-phalanx-proximal";
    case device::mojom::blink::XRHandJoint::kRingFingerPhalanxIntermediate:
      return "ring-finger-phalanx-intermediate";
    case device::mojom::blink::XRHandJoint::kRingFingerPhalanxDistal:
      return "ring-finger-phalanx-distal";
    case device::mojom::blink::XRHandJoint::kRingFingerTip:
      return "ring-finger-tip";
    case device::mojom::blink::XRHandJoint::kPinkyFingerMetacarpal:
      return "pinky-finger-metacarpal";
    case device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxProximal:
      return "pinky-finger-phalanx-proximal";
    case device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxIntermediate:
      return "pinky-finger-phalanx-intermediate";
    case device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxDistal:
      return "pinky-finger-phalanx-distal";
    case device::mojom::blink::XRHandJoint::kPinkyFingerTip:
      return "pinky-finger-tip";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace blink
