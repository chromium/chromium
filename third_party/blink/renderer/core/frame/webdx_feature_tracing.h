// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEBDX_FEATURE_TRACING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEBDX_FEATURE_TRACING_H_

#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class UseCounterFeature;
class LocalFrame;

// Converts a WebDXFeature enum to its web-feature identifier string
// representation (e.g., mojom::blink::WebDXFeature::kViewTransitions ->
// "view-transitions"). For obsolete features, it returns an empty string.
CORE_EXPORT std::string WebDXFeatureEnumToString(
    mojom::blink::WebDXFeature feature);

// Emits a trace event for the given feature if the
// blink.webdx_feature_usage category is enabled.
CORE_EXPORT void MaybeEmitWebDXFeatureTraceEvent(
    const UseCounterFeature& feature,
    const LocalFrame* source_frame);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEBDX_FEATURE_TRACING_H_
