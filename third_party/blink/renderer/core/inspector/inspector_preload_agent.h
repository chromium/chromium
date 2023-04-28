// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PRELOAD_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PRELOAD_AGENT_H_

#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/preload.h"

namespace blink {

class Document;
class SpeculationCandidate;
class SpeculationRuleSet;

namespace internal {

// Exposed for tests.
CORE_EXPORT std::unique_ptr<protocol::Preload::RuleSet> BuildProtocolRuleSet(
    const SpeculationRuleSet& rule_set,
    const String& loader_id);

}  // namespace internal

class CORE_EXPORT InspectorPreloadAgent final
    : public InspectorBaseAgent<protocol::Preload::Metainfo> {
 public:
  InspectorPreloadAgent();
  InspectorPreloadAgent(const InspectorPreloadAgent&) = delete;
  InspectorPreloadAgent& operator=(const InspectorPreloadAgent&) = delete;
  ~InspectorPreloadAgent() override;

  // Probes
  void DidAddSpeculationRuleSet(Document& document,
                                const SpeculationRuleSet& rule_set);
  void DidRemoveSpeculationRuleSet(const SpeculationRuleSet& rule_set);
  void SpeculationCandidatesUpdated(
      Document& document,
      const HeapVector<Member<SpeculationCandidate>>& candidates);

 private:
  void Restore() override;

  // Called from frontend
  protocol::Response enable() override;
  protocol::Response disable() override;

  void EnableInternal();

  InspectorAgentState::Boolean enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PRELOAD_AGENT_H_
