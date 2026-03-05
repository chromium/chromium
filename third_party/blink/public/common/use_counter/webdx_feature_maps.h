// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_WEBDX_FEATURE_MAPS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_WEBDX_FEATURE_MAPS_H_

#include "base/containers/flat_map.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom.h"

namespace blink {

// Map of WebFeature to WebDXFeature.
BLINK_COMMON_EXPORT const
    base::flat_map<mojom::WebFeature, mojom::WebDXFeature>&
    GetWebFeatureToWebDXFeatureMap();

// Map of CSSSampleId to WebDXFeature for CSS properties.
BLINK_COMMON_EXPORT const
    base::flat_map<mojom::CSSSampleId, mojom::WebDXFeature>&
    GetCSSPropertiesToWebDXFeatureMap();

// Map of CSSSampleId to WebDXFeature for animated CSS properties.
BLINK_COMMON_EXPORT const
    base::flat_map<mojom::CSSSampleId, mojom::WebDXFeature>&
    GetAnimatedCSSPropertiesToWebDXFeatureMap();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_WEBDX_FEATURE_MAPS_H_
