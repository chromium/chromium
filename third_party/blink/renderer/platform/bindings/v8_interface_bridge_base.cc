// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_interface_bridge_base.h"

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace bindings {

V8InterfaceBridgeBase::FeatureSelector::FeatureSelector()
    : does_select_all_(true), selector_(OriginTrialFeature::kNonExisting) {}

V8InterfaceBridgeBase::FeatureSelector::FeatureSelector(
    OriginTrialFeature feature)
    : selector_(feature) {}

}  // namespace bindings
}  // namespace blink
