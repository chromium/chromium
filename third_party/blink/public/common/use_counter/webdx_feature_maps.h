// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_WEBDX_FEATURE_MAPS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_WEBDX_FEATURE_MAPS_H_

#include <optional>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom.h"

namespace blink {

// Maps a WebFeature to its corresponding WebDXFeature, if any.
BLINK_COMMON_EXPORT std::optional<mojom::WebDXFeature>
MapWebFeatureToWebDXFeature(mojom::WebFeature feature);

// Maps a CSSSampleId to its corresponding WebDXFeature for CSS properties, if
// any.
BLINK_COMMON_EXPORT std::optional<mojom::WebDXFeature>
MapCSSPropertyToWebDXFeature(mojom::CSSSampleId css_property);

// Maps a CSSSampleId to its corresponding WebDXFeature for animated CSS
// properties, if any.
BLINK_COMMON_EXPORT std::optional<mojom::WebDXFeature>
MapAnimatedCSSPropertyToWebDXFeature(mojom::CSSSampleId css_property);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_USE_COUNTER_WEBDX_FEATURE_MAPS_H_
