// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_ads_agent.h"

#include "third_party/blink/renderer/core/ad_tracker/ad_tracker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/ad_tagging_utils.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"

namespace blink {

InspectorAdsAgent::InspectorAdsAgent(InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

InspectorAdsAgent::~InspectorAdsAgent() = default;

void InspectorAdsAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

protocol::Response InspectorAdsAgent::getAdScripts(
    std::unique_ptr<protocol::Array<protocol::Ads::AdScript>>* out_newScripts) {
  *out_newScripts =
      std::make_unique<protocol::Array<protocol::Ads::AdScript>>();

  LocalFrame* local_root = inspected_frames_->Root();
  CHECK(local_root);
  if (!local_root->GetDocument()) {
    return protocol::Response::Success();
  }

  AdTracker* ad_tracker = AdTracker::FromExecutionContext(
      local_root->GetDocument()->GetExecutionContext());
  if (!ad_tracker) {
    return protocol::Response::Success();
  }

  const HashMap<V8ScriptId, AdProvenance>& ad_scripts =
      ad_tracker->GetAdScripts();

  for (const auto& [script_id, provenance] : ad_scripts) {
    if (!ad_scripts_already_sent_.Contains(script_id)) {
      std::unique_ptr<protocol::Network::AdProvenance> protocol_provenance =
          CreateAdProvenanceProtocolObject(*local_root->GetDocument(),
                                           provenance);
      (*out_newScripts)
          ->emplace_back(protocol::Ads::AdScript::create()
                             .setScriptId(String::Number(script_id.value()))
                             .setProvenance(std::move(protocol_provenance))
                             .build());
      ad_scripts_already_sent_.insert(script_id);
    }
  }

  return protocol::Response::Success();
}

}  // namespace blink
