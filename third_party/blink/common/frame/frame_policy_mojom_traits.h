// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_COMMON_FRAME_FRAME_POLICY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_COMMON_FRAME_FRAME_POLICY_MOJOM_TRAITS_H_

#include "third_party/blink/common/feature_policy/feature_policy_mojom_traits.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/frame/frame_policy.mojom-shared.h"

namespace mojo {

template <>
class BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::FramePolicyDataView, blink::FramePolicy> {
 public:
  static bool allowed_to_download_without_user_activation(
      const blink::FramePolicy& frame_policy) {
    return frame_policy.allowed_to_download_without_user_activation;
  }

  static const std::vector<blink::ParsedFeaturePolicyDeclaration>&
  container_policy(const blink::FramePolicy& frame_policy) {
    return frame_policy.container_policy;
  }

  static blink::WebSandboxFlags sandbox_flags(
      const blink::FramePolicy& frame_policy) {
    return frame_policy.sandbox_flags;
  }

  static bool Read(blink::mojom::FramePolicyDataView in,
                   blink::FramePolicy* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_COMMON_FRAME_FRAME_POLICY_MOJOM_TRAITS_H_
