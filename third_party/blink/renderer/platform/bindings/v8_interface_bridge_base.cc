// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_interface_bridge_base.h"

#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"

namespace blink {
namespace bindings {

V8InterfaceBridgeBase::FeatureSelector::FeatureSelector()
    : does_select_all_(true),
      selector_(blink::mojom::blink::OriginTrialFeature::kNonExisting) {}

V8InterfaceBridgeBase::FeatureSelector::FeatureSelector(
    blink::mojom::blink::OriginTrialFeature feature)
    : selector_(feature) {}

}  // namespace bindings
}  // namespace blink
