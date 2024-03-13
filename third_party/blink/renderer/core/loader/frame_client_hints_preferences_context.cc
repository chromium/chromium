// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.h"

#include <algorithm>

#include "base/no_destructor.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

namespace {

using ClientHintToWebFeatureMap =
    WTF::HashMap<network::mojom::WebClientHintsType, WebFeature>;

ClientHintToWebFeatureMap MakeClientHintToWebFeatureMap() {
  // Mapping from WebClientHintsType to WebFeature. The ordering should match
  // the ordering of enums in WebClientHintsType for readability.
  return {
      {network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
       WebFeature::kClientHintsDeviceMemory_DEPRECATED},
      {network::mojom::WebClientHintsType::kDpr_DEPRECATED,
       WebFeature::kClientHintsDPR_DEPRECATED},
      {network::mojom::WebClientHintsType::kResourceWidth_DEPRECATED,
       WebFeature::kClientHintsResourceWidth_DEPRECATED},
      {network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
       WebFeature::kClientHintsViewportWidth_DEPRECATED},
      {network::mojom::WebClientHintsType::kRtt_DEPRECATED,
       WebFeature::kClientHintsRtt_DEPRECATED},
      {network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
       WebFeature::kClientHintsDownlink_DEPRECATED},
      {network::mojom::WebClientHintsType::kEct_DEPRECATED,
       WebFeature::kClientHintsEct_DEPRECATED},
      {network::mojom::WebClientHintsType::kUA, WebFeature::kClientHintsUA},
      {network::mojom::WebClientHintsType::kUAArch,
       WebFeature::kClientHintsUAArch},
      {network::mojom::WebClientHintsType::kUAPlatform,
       WebFeature::kClientHintsUAPlatform},
      {network::mojom::WebClientHintsType::kUAModel,
       WebFeature::kClientHintsUAModel},
      {network::mojom::WebClientHintsType::kUAMobile,
       WebFeature::kClientHintsUAMobile},
      {network::mojom::WebClientHintsType::kUAFullVersion,
       WebFeature::kClientHintsUAFullVersion},
      {network::mojom::WebClientHintsType::kUAPlatformVersion,
       WebFeature::kClientHintsUAPlatformVersion},
      {network::mojom::WebClientHintsType::kPrefersColorScheme,
       WebFeature::kClientHintsPrefersColorScheme},
      {network::mojom::WebClientHintsType::kUABitness,
       WebFeature::kClientHintsUABitness},
      {network::mojom::WebClientHintsType::kViewportHeight,
       WebFeature::kClientHintsViewportHeight},
      {network::mojom::WebClientHintsType::kDeviceMemory,
       WebFeature::kClientHintsDeviceMemory},
      {network::mojom::WebClientHintsType::kDpr, WebFeature::kClientHintsDPR},
      {network::mojom::WebClientHintsType::kResourceWidth,
       WebFeature::kClientHintsResourceWidth},
      {network::mojom::WebClientHintsType::kViewportWidth,
       WebFeature::kClientHintsViewportWidth},
      {network::mojom::WebClientHintsType::kUAFullVersionList,
       WebFeature::kClientHintsUAFullVersionList},
      {network::mojom::WebClientHintsType::kUAWoW64,
       WebFeature::kClientHintsUAWoW64},
      {network::mojom::WebClientHintsType::kSaveData,
       WebFeature::kClientHintsSaveData},
      {network::mojom::WebClientHintsType::kPrefersReducedMotion,
       WebFeature::kClientHintsPrefersReducedMotion},
      {network::mojom::WebClientHintsType::kUAFormFactors,
       WebFeature::kClientHintsUAFormFactors},
      {network::mojom::WebClientHintsType::kPrefersReducedTransparency,
       WebFeature::kClientHintsPrefersReducedTransparency},
  };
}

const ClientHintToWebFeatureMap& GetClientHintToWebFeatureMap() {
  DCHECK_EQ(network::GetClientHintToNameMap().size(),
            MakeClientHintToWebFeatureMap().size());
  static const base::NoDestructor<ClientHintToWebFeatureMap> map(
      MakeClientHintToWebFeatureMap());
  return *map;
}

}  // namespace

FrameClientHintsPreferencesContext::FrameClientHintsPreferencesContext(
    LocalFrame* frame)
    : frame_(frame) {}

ukm::SourceId FrameClientHintsPreferencesContext::GetUkmSourceId() {
  return frame_->GetDocument()->UkmSourceID();
}

ukm::UkmRecorder* FrameClientHintsPreferencesContext::GetUkmRecorder() {
  return frame_->GetDocument()->UkmRecorder();
}

void FrameClientHintsPreferencesContext::CountClientHints(
    network::mojom::WebClientHintsType type) {
  UseCounter::Count(*frame_->GetDocument(),
                    GetClientHintToWebFeatureMap().at(type));
}

}  // namespace blink
