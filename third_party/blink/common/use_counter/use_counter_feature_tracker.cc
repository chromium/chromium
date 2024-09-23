// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/use_counter/use_counter_feature_tracker.h"

namespace blink {
namespace {
template <size_t N>
bool BitsetContains(const std::bitset<N>& lhs, const std::bitset<N>& rhs) {
  return (lhs & rhs) == rhs;
}
}  // namespace

using FeatureType = mojom::UseCounterFeatureType;

bool UseCounterFeatureTracker::Test(const UseCounterFeature& feature) const {
  switch (feature.type()) {
    case FeatureType::kWebFeature:
      return web_features_.test(feature.value());
    case FeatureType::kWebDXFeature:
      return webdx_features_.test(feature.value());
    case FeatureType::kCssProperty:
      return css_properties_.test(feature.value());
    case FeatureType::kAnimatedCssProperty:
      return animated_css_properties_.test(feature.value());
    case FeatureType::kPermissionsPolicyViolationEnforce:
      return violated_permissions_policy_features_.test(feature.value());
    case FeatureType::kPermissionsPolicyIframeAttribute:
      return iframe_permissions_policy_features_.test(feature.value());
    case FeatureType::kPermissionsPolicyHeader:
      return header_permissions_policy_features_.test(feature.value());
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

  for (uint32_t i = 0; i < iframe_permissions_policy_features_.size(); i++) {
    if (iframe_permissions_policy_features_.test(i))
      ret.push_back({FeatureType::kPermissionsPolicyIframeAttribute, i});
  }

  for (uint32_t i = 0; i < header_permissions_policy_features_.size(); i++) {
    if (header_permissions_policy_features_.test(i))
      ret.push_back({FeatureType::kPermissionsPolicyHeader, i});
  }

  return ret;
}

void UseCounterFeatureTracker::ResetForTesting(
    const UseCounterFeature& feature) {
  Set(feature, false);
}

bool UseCounterFeatureTracker::ContainsForTesting(
    const UseCounterFeatureTracker& other) const {
  return BitsetContains(web_features_, other.web_features_) &&
         BitsetContains(css_properties_, other.css_properties_) &&
         BitsetContains(animated_css_properties_,
                        other.animated_css_properties_);
}

void UseCounterFeatureTracker::Set(const UseCounterFeature& feature,
                                   bool value) {
  switch (feature.type()) {
    case FeatureType::kWebFeature:
      web_features_[feature.value()] = value;
      break;
    case FeatureType::kWebDXFeature:
      webdx_features_[feature.value()] = value;
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
    case FeatureType::kPermissionsPolicyIframeAttribute:
      iframe_permissions_policy_features_[feature.value()] = value;
      break;
    case FeatureType::kPermissionsPolicyHeader:
      header_permissions_policy_features_[feature.value()] = value;
      break;
  }
}

}  // namespace blink
