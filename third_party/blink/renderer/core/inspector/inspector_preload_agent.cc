// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_preload_agent.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"

namespace blink {

using protocol::Response;

namespace {

std::unique_ptr<protocol::Preload::RuleSet> BuildProtocolRuleSet(
    const SpeculationRuleSet& rule_set,
    const String& loader_id) {
  return protocol::Preload::RuleSet::create()
      .setId(rule_set.InspectorId())
      .setLoaderId(loader_id)
      .setSourceText(rule_set.source()->GetSourceText())
      .build();
}

}  // namespace

InspectorPreloadAgent::InspectorPreloadAgent()
    : enabled_(&agent_state_, /*default_value=*/false) {}

InspectorPreloadAgent::~InspectorPreloadAgent() = default;

void InspectorPreloadAgent::Restore() {
  if (enabled_.Get()) {
    EnableInternal();
  }
}

void InspectorPreloadAgent::DidAddSpeculationRuleSet(
    Document& document,
    const SpeculationRuleSet& rule_set) {
  if (!enabled_.Get()) {
    return;
  }

  String loader_id = IdentifiersFactory::LoaderId(document.Loader());
  GetFrontend()->ruleSetUpdated(BuildProtocolRuleSet(rule_set, loader_id));
}

void InspectorPreloadAgent::DidRemoveSpeculationRuleSet(
    const SpeculationRuleSet& rule_set) {
  if (!enabled_.Get()) {
    return;
  }

  GetFrontend()->ruleSetRemoved(rule_set.InspectorId());
}

protocol::Response InspectorPreloadAgent::enable() {
  EnableInternal();
  return Response::Success();
}

protocol::Response InspectorPreloadAgent::disable() {
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorPreloadAgent(this);
  return Response::Success();
}

void InspectorPreloadAgent::EnableInternal() {
  DCHECK(GetFrontend());

  enabled_.Set(true);
  instrumenting_agents_->AddInspectorPreloadAgent(this);
}

}  // namespace blink
