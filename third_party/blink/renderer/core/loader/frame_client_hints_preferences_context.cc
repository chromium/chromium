// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

// Mapping from WebClientHintsType to WebFeature. The ordering should match the
// ordering of enums in WebClientHintsType.
static constexpr WebFeature kWebFeatureMapping[] = {
    WebFeature::kClientHintsDeviceMemory,
    WebFeature::kClientHintsDPR,
    WebFeature::kClientHintsResourceWidth,
    WebFeature::kClientHintsViewportWidth,
    WebFeature::kClientHintsRtt,
    WebFeature::kClientHintsDownlink,
    WebFeature::kClientHintsEct,
    WebFeature::kClientHintsLang,
    WebFeature::kClientHintsUA,
    WebFeature::kClientHintsUAArch,
    WebFeature::kClientHintsUAPlatform,
    WebFeature::kClientHintsUAModel,
    WebFeature::kClientHintsUAMobile,
    WebFeature::kClientHintsUAFullVersion,
    WebFeature::kClientHintsUAPlatformVersion,
    WebFeature::kClientHintsPrefersColorScheme,
    WebFeature::kClientHintsUABitness,
};

static_assert(static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) +
                      1 ==
                  base::size(kWebFeatureMapping),
              "unhandled client hint type");

}  // namespace

FrameClientHintsPreferencesContext::FrameClientHintsPreferencesContext(
    LocalFrame* frame)
    : frame_(frame) {}

void FrameClientHintsPreferencesContext::CountClientHints(
    network::mojom::WebClientHintsType type) {
  UseCounter::Count(*frame_->GetDocument(),
                    kWebFeatureMapping[static_cast<int32_t>(type)]);
}

}  // namespace blink
