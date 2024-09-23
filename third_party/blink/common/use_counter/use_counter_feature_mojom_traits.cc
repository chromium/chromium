// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/use_counter/use_counter_feature_mojom_traits.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom-shared.h"

namespace mojo {
namespace {
// There are reserved features that should NOT be passed through mojom
// interface. Returns true if the feature is reserved.
bool IsReservedFeature(const blink::UseCounterFeature& feature) {
  switch (feature.type()) {
    case blink::mojom::UseCounterFeatureType::kWebFeature:
      return feature.value() ==
             static_cast<blink::UseCounterFeature::EnumValue>(
                 blink::mojom::WebFeature::kPageVisits);
    case blink::mojom::UseCounterFeatureType::kWebDXFeature:
      return feature.value() ==
             static_cast<blink::UseCounterFeature::EnumValue>(
                 blink::mojom::WebDXFeature::kPageVisits);
    case blink::mojom::UseCounterFeatureType::kCssProperty:
    case blink::mojom::UseCounterFeatureType::kAnimatedCssProperty:
      return feature.value() ==
             static_cast<blink::UseCounterFeature::EnumValue>(
                 blink::mojom::CSSSampleId::kTotalPagesMeasured);
    case blink::mojom::UseCounterFeatureType::
        kPermissionsPolicyViolationEnforce:
    case blink::mojom::UseCounterFeatureType::kPermissionsPolicyHeader:
    case blink::mojom::UseCounterFeatureType::kPermissionsPolicyIframeAttribute:
      return feature.value() ==
             static_cast<blink::UseCounterFeature::EnumValue>(
                 blink::mojom::PermissionsPolicyFeature::kNotFound);
  }
}
}  // namespace

bool StructTraits<
    blink::mojom::UseCounterFeatureDataView,
    blink::UseCounterFeature>::Read(blink::mojom::UseCounterFeatureDataView in,
                                    blink::UseCounterFeature* out) {
  return out->SetTypeAndValue(in.type(), in.value()) &&
         !IsReservedFeature(*out);
}

}  // namespace mojo
