// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_PARSER_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_PARSER_DELEGATE_H_

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class FeaturePolicyParserDelegate : public FeatureContext {
 public:
  virtual void CountFeaturePolicyUsage(mojom::WebFeature feature) = 0;
  virtual bool FeaturePolicyFeatureObserved(
      mojom::FeaturePolicyFeature feature) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_FEATURE_POLICY_PARSER_DELEGATE_H_
