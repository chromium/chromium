// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/use_counter/use_counter_feature_tracker.h"

namespace blink {

using FeatureType = mojom::UseCounterFeatureType;

bool UseCounterFeatureTracker::Test(const UseCounterFeature& feature) const {
  switch (feature.type()) {
    case FeatureType::kWebFeature:
      return web_features_.test(feature.value());
    case FeatureType::kCssProperty:
      return css_properties_.test(feature.value());
    case FeatureType::kAnimatedCssProperty:
      return animated_css_properties_.test(feature.value());
    case FeatureType::kPermissionsPolicyViolationEnforce:
      return violated_permissions_policy_features_.test(feature.value());
  }
}

bool UseCounterFeatureTracker::TestAndSet(const UseCounterFeature& feature) {
  bool has_record = Test(feature);
  Set(feature, true);
  return has_record;
}

std::vector<UseCounterFeature> UseCounterFeatureTracker::GetRecordedFeatures()
    const {
  std::vector<UseCounterFeature> ret;
  for (uint32_t i = 0; i < web_features_.size(); i++) {
    if (web_features_.test(i))
      ret.push_back({FeatureType::kWebFeature, i});
  }

  for (uint32_t i = 0; i < css_properties_.size(); i++) {
    if (css_properties_.test(i))
      ret.push_back({FeatureType::kCssProperty, i});
  }

  for (uint32_t i = 0; i < animated_css_properties_.size(); i++) {
    if (animated_css_properties_.test(i))
      ret.push_back({FeatureType::kAnimatedCssProperty, i});
  }

  for (uint32_t i = 0; i < violated_permissions_policy_features_.size(); i++) {
    if (violated_permissions_policy_features_.test(i))
      ret.push_back({FeatureType::kPermissionsPolicyViolationEnforce, i});
  }
  return ret;
}

void UseCounterFeatureTracker::ResetForTesting(
    const UseCounterFeature& feature) {
  Set(feature, false);
}

void UseCounterFeatureTracker::Set(const UseCounterFeature& feature,
                                   bool value) {
  switch (feature.type()) {
    case FeatureType::kWebFeature:
      web_features_[feature.value()] = value;
      break;
    case FeatureType::kCssProperty:
      css_properties_[feature.value()] = value;
      break;
    case FeatureType::kAnimatedCssProperty:
      animated_css_properties_[feature.value()] = value;
      break;
    case FeatureType::kPermissionsPolicyViolationEnforce:
      violated_permissions_policy_features_[feature.value()] = value;
      break;
  }
}

}  // namespace blink
