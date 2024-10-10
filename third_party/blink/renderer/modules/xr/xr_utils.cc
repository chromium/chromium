// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_utils.h"

#include <cmath>

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

DOMFloat32Array* transformationMatrixToDOMFloat32Array(
    const gfx::Transform& matrix) {
  float array[16];
  matrix.GetColMajorF(array);
  return DOMFloat32Array::Create(array);
}

gfx::Transform DOMFloat32ArrayToTransform(DOMFloat32Array* m) {
  DCHECK_EQ(m->length(), 16u);
  return gfx::Transform::ColMajorF(m->Data());
}

gfx::Transform WTFFloatVectorToTransform(const Vector<float>& m) {
  DCHECK_EQ(m.size(), 16u);
  return gfx::Transform::ColMajorF(m.data());
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
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::optional<device::Pose> CreatePose(const gfx::Transform& matrix) {
  return device::Pose::Create(matrix);
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

  NOTREACHED_IN_MIGRATION();
  return device::mojom::blink::XRHandJoint::kMaxValue;
}

V8XRHandJoint::Enum MojomHandJointToV8Enum(
    device::mojom::blink::XRHandJoint hand_joint) {
  switch (hand_joint) {
    case device::mojom::blink::XRHandJoint::kWrist:
      return V8XRHandJoint::Enum::kWrist;
    case device::mojom::blink::XRHandJoint::kThumbMetacarpal:
      return V8XRHandJoint::Enum::kThumbMetacarpal;
    case device::mojom::blink::XRHandJoint::kThumbPhalanxProximal:
      return V8XRHandJoint::Enum::kThumbPhalanxProximal;
    case device::mojom::blink::XRHandJoint::kThumbPhalanxDistal:
      return V8XRHandJoint::Enum::kThumbPhalanxDistal;
    case device::mojom::blink::XRHandJoint::kThumbTip:
      return V8XRHandJoint::Enum::kThumbTip;
    case device::mojom::blink::XRHandJoint::kIndexFingerMetacarpal:
      return V8XRHandJoint::Enum::kIndexFingerMetacarpal;
    case device::mojom::blink::XRHandJoint::kIndexFingerPhalanxProximal:
      return V8XRHandJoint::Enum::kIndexFingerPhalanxProximal;
    case device::mojom::blink::XRHandJoint::kIndexFingerPhalanxIntermediate:
      return V8XRHandJoint::Enum::kIndexFingerPhalanxIntermediate;
    case device::mojom::blink::XRHandJoint::kIndexFingerPhalanxDistal:
      return V8XRHandJoint::Enum::kIndexFingerPhalanxDistal;
    case device::mojom::blink::XRHandJoint::kIndexFingerTip:
      return V8XRHandJoint::Enum::kIndexFingerTip;
    case device::mojom::blink::XRHandJoint::kMiddleFingerMetacarpal:
      return V8XRHandJoint::Enum::kMiddleFingerMetacarpal;
    case device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxProximal:
      return V8XRHandJoint::Enum::kMiddleFingerPhalanxProximal;
    case device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxIntermediate:
      return V8XRHandJoint::Enum::kMiddleFingerPhalanxIntermediate;
    case device::mojom::blink::XRHandJoint::kMiddleFingerPhalanxDistal:
      return V8XRHandJoint::Enum::kMiddleFingerPhalanxDistal;
    case device::mojom::blink::XRHandJoint::kMiddleFingerTip:
      return V8XRHandJoint::Enum::kMiddleFingerTip;
    case device::mojom::blink::XRHandJoint::kRingFingerMetacarpal:
      return V8XRHandJoint::Enum::kRingFingerMetacarpal;
    case device::mojom::blink::XRHandJoint::kRingFingerPhalanxProximal:
      return V8XRHandJoint::Enum::kRingFingerPhalanxProximal;
    case device::mojom::blink::XRHandJoint::kRingFingerPhalanxIntermediate:
      return V8XRHandJoint::Enum::kRingFingerPhalanxIntermediate;
    case device::mojom::blink::XRHandJoint::kRingFingerPhalanxDistal:
      return V8XRHandJoint::Enum::kRingFingerPhalanxDistal;
    case device::mojom::blink::XRHandJoint::kRingFingerTip:
      return V8XRHandJoint::Enum::kRingFingerTip;
    case device::mojom::blink::XRHandJoint::kPinkyFingerMetacarpal:
      return V8XRHandJoint::Enum::kPinkyFingerMetacarpal;
    case device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxProximal:
      return V8XRHandJoint::Enum::kPinkyFingerPhalanxProximal;
    case device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxIntermediate:
      return V8XRHandJoint::Enum::kPinkyFingerPhalanxIntermediate;
    case device::mojom::blink::XRHandJoint::kPinkyFingerPhalanxDistal:
      return V8XRHandJoint::Enum::kPinkyFingerPhalanxDistal;
    case device::mojom::blink::XRHandJoint::kPinkyFingerTip:
      return V8XRHandJoint::Enum::kPinkyFingerTip;
  }
  NOTREACHED();
}

std::optional<device::mojom::XRSessionFeature> StringToXRSessionFeature(
    const String& feature_string) {
  if (feature_string == "viewer") {
    return device::mojom::XRSessionFeature::REF_SPACE_VIEWER;
  } else if (feature_string == "local") {
    return device::mojom::XRSessionFeature::REF_SPACE_LOCAL;
  } else if (feature_string == "local-floor") {
    return device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR;
  } else if (feature_string == "bounded-floor") {
    return device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR;
  } else if (feature_string == "unbounded") {
    return device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED;
  } else if (feature_string == "hit-test") {
    return device::mojom::XRSessionFeature::HIT_TEST;
  } else if (feature_string == "anchors") {
    return device::mojom::XRSessionFeature::ANCHORS;
  } else if (feature_string == "dom-overlay") {
    return device::mojom::XRSessionFeature::DOM_OVERLAY;
  } else if (feature_string == "light-estimation") {
    return device::mojom::XRSessionFeature::LIGHT_ESTIMATION;
  } else if (feature_string == "camera-access") {
    return device::mojom::XRSessionFeature::CAMERA_ACCESS;
  } else if (feature_string == "plane-detection") {
    return device::mojom::XRSessionFeature::PLANE_DETECTION;
  } else if (feature_string == "depth-sensing") {
    return device::mojom::XRSessionFeature::DEPTH;
  } else if (feature_string == "image-tracking") {
    return device::mojom::XRSessionFeature::IMAGE_TRACKING;
  } else if (feature_string == "hand-tracking") {
    return device::mojom::XRSessionFeature::HAND_INPUT;
  } else if (feature_string == "secondary-views") {
    return device::mojom::XRSessionFeature::SECONDARY_VIEWS;
  } else if (feature_string == "layers") {
    return device::mojom::XRSessionFeature::LAYERS;
  } else if (feature_string == "front-facing") {
    return device::mojom::XRSessionFeature::FRONT_FACING;
  } else if (feature_string == "webgpu") {
    return device::mojom::XRSessionFeature::WEBGPU;
  }

  return std::nullopt;
}

String XRSessionFeatureToString(device::mojom::XRSessionFeature feature) {
  switch (feature) {
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
      return "viewer";
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
      return "local";
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      return "local-floor";
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
      return "bounded-floor";
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
      return "unbounded";
    case device::mojom::XRSessionFeature::DOM_OVERLAY:
      return "dom-overlay";
    case device::mojom::XRSessionFeature::HIT_TEST:
      return "hit-test";
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
      return "light-estimation";
    case device::mojom::XRSessionFeature::ANCHORS:
      return "anchors";
    case device::mojom::XRSessionFeature::CAMERA_ACCESS:
      return "camera-access";
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
      return "plane-detection";
    case device::mojom::XRSessionFeature::DEPTH:
      return "depth-sensing";
    case device::mojom::XRSessionFeature::IMAGE_TRACKING:
      return "image-tracking";
    case device::mojom::XRSessionFeature::HAND_INPUT:
      return "hand-tracking";
    case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
      return "secondary-views";
    case device::mojom::XRSessionFeature::LAYERS:
      return "layers";
    case device::mojom::XRSessionFeature::FRONT_FACING:
      return "front-facing";
    case device::mojom::XRSessionFeature::WEBGPU:
      return "webgpu";
  }

  return "";
}

bool IsFeatureEnabledForContext(device::mojom::XRSessionFeature feature,
                                const ExecutionContext* context) {
  switch (feature) {
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
      return RuntimeEnabledFeatures::WebXRPlaneDetectionEnabled(context);
    case device::mojom::XRSessionFeature::IMAGE_TRACKING:
      return RuntimeEnabledFeatures::WebXRImageTrackingEnabled(context);
    case device::mojom::XRSessionFeature::HAND_INPUT:
      return RuntimeEnabledFeatures::WebXRHandInputEnabled(context);
    case device::mojom::XRSessionFeature::LAYERS:
    case device::mojom::XRSessionFeature::WEBGPU:
      return RuntimeEnabledFeatures::WebXRLayersEnabled(context);
    case device::mojom::XRSessionFeature::FRONT_FACING:
      return RuntimeEnabledFeatures::WebXRFrontFacingEnabled(context);
    case device::mojom::XRSessionFeature::HIT_TEST:
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
    case device::mojom::XRSessionFeature::ANCHORS:
    case device::mojom::XRSessionFeature::CAMERA_ACCESS:
    case device::mojom::XRSessionFeature::DEPTH:
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
    case device::mojom::XRSessionFeature::DOM_OVERLAY:
    case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
      return true;
  }
}

}  // namespace blink
