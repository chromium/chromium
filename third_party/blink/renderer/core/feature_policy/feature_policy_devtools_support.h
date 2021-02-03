// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_DEVTOOLS_SUPPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_DEVTOOLS_SUPPORT_H_

#include "base/optional.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class Frame;

// The reason for a feature to be disallowed.
enum class FeaturePolicyBlockReason {
  // Feature's allowlist declaration can be overridden either in HTTP header,
  // or in iframe attribute.
  kHeader,
  kIframeAttribute,
};

struct FeaturePolicyBlockLocator {
  // FrameId used in devtools protocol.
  String frame_id;
  // Note: Attribute declaration is on frame's owner element, which is
  // technically above 1 level in the frame tree.
  FeaturePolicyBlockReason reason;
};

// Traces the root reason for a feature to be disabled in a frame.
// Returns base::nullopt when the feature is enabled in the frame.
CORE_EXPORT base::Optional<FeaturePolicyBlockLocator>
TraceFeaturePolicyBlockSource(Frame*, mojom::FeaturePolicyFeature);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_DEVTOOLS_SUPPORT_H_
