// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ADS_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ADS_AGENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/ads.h"
#include "third_party/blink/renderer/platform/loader/fetch/ad_tagging_utils.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "v8/include/v8.h"

namespace blink {

class InspectedFrames;

// Implements the "Ads" DevTools protocol domain in the renderer process.
//
// This agent handles Ads domain commands that require access to
// renderer-process state, such as ad scripts tracking data. Commands requiring
// browser-process state (like ad metrics) are handled separately by the
// browser-side `AdsHandler`.
class CORE_EXPORT InspectorAdsAgent final
    : public InspectorBaseAgent<protocol::Ads::Metainfo> {
 public:
  explicit InspectorAdsAgent(InspectedFrames* inspected_frames);
  InspectorAdsAgent(const InspectorAdsAgent&) = delete;
  InspectorAdsAgent& operator=(const InspectorAdsAgent&) = delete;
  ~InspectorAdsAgent() override;

  void Trace(Visitor*) const override;

  // protocol::Ads::Backend implementation:
  protocol::Response getAdScripts(
      std::unique_ptr<protocol::Array<protocol::Ads::AdScript>>* out_newScripts)
      override;

 private:
  // The frames that are inspected. The `Root()` frame is the local root of the
  // target.
  Member<InspectedFrames> inspected_frames_;

  // Tracks ad scripts that have already been sent to the frontend.
  // Note that the tracked ad scripts on the AdTracker will never be deleted,
  // so this set only grows and ensures we only send newly discovered scripts.
  HashSet<V8ScriptId> ad_scripts_already_sent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ADS_AGENT_H_
