// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/use_counter/use_counter_feature.h"

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom-shared.h"

namespace blink {

UseCounterFeature::UseCounterFeature(mojom::UseCounterFeatureType type,
                                     EnumValue value)
    : type_(type), value_(value) {
  DCHECK(IsValid());
}

bool UseCounterFeature::SetTypeAndValue(mojom::UseCounterFeatureType type,
                                        EnumValue value) {
  type_ = type;
  value_ = value;
  return IsValid();
}

bool UseCounterFeature::IsValid() const {
  switch (type_) {
    case mojom::UseCounterFeatureType::kWebFeature:
      return value_ <= static_cast<UseCounterFeature::EnumValue>(
                           mojom::WebFeature::kMaxValue);
    case mojom::UseCounterFeatureType::kWebDXFeature:
      return value_ <= static_cast<UseCounterFeature::EnumValue>(
                           mojom::WebDXFeature::kMaxValue);
    case mojom::UseCounterFeatureType::kCssProperty:
    case mojom::UseCounterFeatureType::kAnimatedCssProperty:
      return value_ <= static_cast<UseCounterFeature::EnumValue>(
                           mojom::CSSSampleId::kMaxValue);
    case mojom::UseCounterFeatureType::kPermissionsPolicyViolationEnforce:
    case mojom::UseCounterFeatureType::kPermissionsPolicyHeader:
    case mojom::UseCounterFeatureType::kPermissionsPolicyIframeAttribute:
      return value_ <= static_cast<UseCounterFeature::EnumValue>(
                           mojom::PermissionsPolicyFeature::kMaxValue);
  }
}

bool UseCounterFeature::operator==(const UseCounterFeature& rhs) const {
  return std::tie(type_, value_) == std::tie(rhs.type_, rhs.value_);
}

bool UseCounterFeature::operator<(const UseCounterFeature& rhs) const {
  return std::tie(type_, value_) < std::tie(rhs.type_, rhs.value_);
}

}  // namespace blink
