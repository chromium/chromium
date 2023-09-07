// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_preload_agent.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_candidate.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"

namespace blink {

namespace {

absl::optional<protocol::Preload::RuleSetErrorType> GetProtocolRuleSetErrorType(
    SpeculationRuleSetErrorType error_type) {
  switch (error_type) {
    case SpeculationRuleSetErrorType::kNoError:
      return absl::nullopt;
    case SpeculationRuleSetErrorType::kSourceIsNotJsonObject:
      return protocol::Preload::RuleSetErrorTypeEnum::SourceIsNotJsonObject;
    case SpeculationRuleSetErrorType::kInvalidRulesSkipped:
      return protocol::Preload::RuleSetErrorTypeEnum::InvalidRulesSkipped;
  }
}

String GetProtocolRuleSetErrorMessage(const SpeculationRuleSet& rule_set) {
  switch (rule_set.error_type()) {
    case SpeculationRuleSetErrorType::kNoError:
      return String();
    case SpeculationRuleSetErrorType::kSourceIsNotJsonObject:
    case SpeculationRuleSetErrorType::kInvalidRulesSkipped:
      return rule_set.error_message();
  }
}

// Struct to represent a unique preloading attempt (corresponds to
// protocol::Preload::PreloadingAttemptKey). Multiple SpeculationCandidates
// could correspond to a single PreloadingAttemptKey.
struct PreloadingAttemptKey {
  mojom::blink::SpeculationAction action;
  KURL url;
  mojom::blink::SpeculationTargetHint target_hint;
};

bool operator==(const PreloadingAttemptKey& a, const PreloadingAttemptKey& b) {
  return std::tie(a.action, a.url, a.target_hint) ==
         std::tie(b.action, b.url, b.target_hint);
}

struct PreloadingAttemptKeyHashTraits
    : WTF::GenericHashTraits<PreloadingAttemptKey> {
  static unsigned GetHash(const PreloadingAttemptKey& key) {
    unsigned hash = WTF::GetHash(key.action);
    hash = WTF::HashInts(hash, WTF::GetHash(key.url));
    hash = WTF::HashInts(hash, WTF::GetHash(key.target_hint));
    return hash;
  }

  static const bool kEmptyValueIsZero = false;

  static PreloadingAttemptKey EmptyValue() {
    return {mojom::blink::SpeculationAction::kPrefetch, KURL(),
            mojom::blink::SpeculationTargetHint::kNoHint};
  }

  static bool IsDeletedValue(const PreloadingAttemptKey& key) {
    const PreloadingAttemptKey deleted_value = {
        mojom::blink::SpeculationAction::kPrerender, KURL(),
        mojom::blink::SpeculationTargetHint::kNoHint};
    return key == deleted_value;
  }

  static void ConstructDeletedValue(PreloadingAttemptKey& slot) {
    new (&slot) PreloadingAttemptKey{
        mojom::blink::SpeculationAction::kPrerender, KURL(),
        mojom::blink::SpeculationTargetHint::kNoHint};
  }
};

protocol::Preload::SpeculationAction GetProtocolSpeculationAction(
    mojom::blink::SpeculationAction action) {
  switch (action) {
    case mojom::blink::SpeculationAction::kPrerender:
      return protocol::Preload::SpeculationActionEnum::Prerender;
    case mojom::blink::SpeculationAction::kPrefetch:
      return protocol::Preload::SpeculationActionEnum::Prefetch;
    case mojom::blink::SpeculationAction::kPrefetchWithSubresources:
      NOTREACHED();
      return String();
  }
}

absl::optional<protocol::Preload::SpeculationTargetHint>
GetProtocolSpeculationTargetHint(
    mojom::blink::SpeculationTargetHint target_hint) {
  switch (target_hint) {
    case mojom::blink::SpeculationTargetHint::kNoHint:
      return absl::nullopt;
    case mojom::blink::SpeculationTargetHint::kSelf:
      return protocol::Preload::SpeculationTargetHintEnum::Self;
    case mojom::blink::SpeculationTargetHint::kBlank:
      return protocol::Preload::SpeculationTargetHintEnum::Blank;
  }
}

std::unique_ptr<protocol::Preload::PreloadingAttemptKey>
BuildProtocolPreloadingAttemptKey(const PreloadingAttemptKey& key,
                                  const Document& document) {
  auto preloading_attempt_key =
      protocol::Preload::PreloadingAttemptKey::create()
          .setLoaderId(IdentifiersFactory::LoaderId(document.Loader()))
          .setAction(GetProtocolSpeculationAction(key.action))
          .setUrl(key.url)
          .build();
  absl::optional<String> target_hint_str =
      GetProtocolSpeculationTargetHint(key.target_hint);
  if (target_hint_str) {
    preloading_attempt_key->setTargetHint(target_hint_str.value());
  }
  return preloading_attempt_key;
}

std::unique_ptr<protocol::Preload::PreloadingAttemptSource>
BuildProtocolPreloadingAttemptSource(
    const PreloadingAttemptKey& key,
    const HeapVector<Member<SpeculationCandidate>>& candidates,
    Document& document) {
  auto preloading_attempt_key =
      BuildProtocolPreloadingAttemptKey(key, document);

  HeapHashSet<Member<SpeculationRuleSet>> unique_rule_sets;
  HeapHashSet<Member<HTMLAnchorElement>> unique_anchors;
  auto rule_set_ids = std::make_unique<protocol::Array<String>>();
  auto node_ids = std::make_unique<protocol::Array<int>>();
  for (SpeculationCandidate* candidate : candidates) {
    if (unique_rule_sets.insert(candidate->rule_set()).is_new_entry) {
      rule_set_ids->push_back(candidate->rule_set()->InspectorId());
    }
    if (HTMLAnchorElement* anchor = candidate->anchor();
        anchor && unique_anchors.insert(anchor).is_new_entry) {
      node_ids->push_back(anchor->GetDomNodeId());
    }
  }
  return protocol::Preload::PreloadingAttemptSource::create()
      .setKey(std::move(preloading_attempt_key))
      .setRuleSetIds(std::move(rule_set_ids))
      .setNodeIds(std::move(node_ids))
      .build();
}

}  // namespace

namespace internal {

std::unique_ptr<protocol::Preload::RuleSet> BuildProtocolRuleSet(
    const SpeculationRuleSet& rule_set,
    const String& loader_id) {
  auto builder = protocol::Preload::RuleSet::create()
                     .setId(rule_set.InspectorId())
                     .setLoaderId(loader_id)
                     .setSourceText(rule_set.source()->GetSourceText())
                     .build();

  auto* source = rule_set.source();
  auto node_id = source->GetNodeId();
  auto url = source->GetSourceURL();
  auto request_id = source->GetRequestId();
  // Ensured by SpeculationRuleSet's ctor.
  CHECK_NE(node_id.has_value(), url.has_value() && request_id.has_value());
  if (node_id.has_value()) {
    builder->setBackendNodeId(node_id.value());
  } else {
    builder->setUrl(url.value());

    String request_id_string =
        IdentifiersFactory::SubresourceRequestId(request_id.value());
    if (!request_id_string.IsNull()) {
      builder->setRequestId(request_id_string);
    }
  }

  if (auto error_type = GetProtocolRuleSetErrorType(rule_set.error_type())) {
    builder->setErrorType(error_type.value());
    builder->setErrorMessage(GetProtocolRuleSetErrorMessage(rule_set));
  }

  return builder;
}

}  // namespace internal

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
  GetFrontend()->ruleSetUpdated(
      internal::BuildProtocolRuleSet(rule_set, loader_id));
}

void InspectorPreloadAgent::DidRemoveSpeculationRuleSet(
    const SpeculationRuleSet& rule_set) {
  if (!enabled_.Get()) {
    return;
  }

  GetFrontend()->ruleSetRemoved(rule_set.InspectorId());
}

void InspectorPreloadAgent::SpeculationCandidatesUpdated(
    Document& document,
    const HeapVector<Member<SpeculationCandidate>>& candidates) {
  if (!enabled_.Get()) {
    return;
  }

  HeapHashMap<PreloadingAttemptKey,
              Member<HeapVector<Member<SpeculationCandidate>>>,
              PreloadingAttemptKeyHashTraits>
      preloading_attempts;
  for (SpeculationCandidate* candidate : candidates) {
    // We are explicitly not reporting candidates for kPrefetchWithSubresources
    // to clients, they are currently only interested in kPrefetch and
    // kPrerender.
    if (candidate->action() ==
        mojom::blink::SpeculationAction::kPrefetchWithSubresources) {
      continue;
    }
    PreloadingAttemptKey key = {candidate->action(), candidate->url(),
                                candidate->target_hint()};
    auto& value = preloading_attempts.insert(key, nullptr).stored_value->value;
    if (!value) {
      value = MakeGarbageCollected<HeapVector<Member<SpeculationCandidate>>>();
    }
    value->push_back(candidate);
  }

  auto preloading_attempt_sources = std::make_unique<
      protocol::Array<protocol::Preload::PreloadingAttemptSource>>();
  for (auto it : preloading_attempts) {
    preloading_attempt_sources->push_back(
        BuildProtocolPreloadingAttemptSource(it.key, *(it.value), document));
  }

  GetFrontend()->preloadingAttemptSourcesUpdated(
      IdentifiersFactory::LoaderId(document.Loader()),
      std::move(preloading_attempt_sources));
}

protocol::Response InspectorPreloadAgent::enable() {
  EnableInternal();
  return protocol::Response::Success();
}

protocol::Response InspectorPreloadAgent::disable() {
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorPreloadAgent(this);
  return protocol::Response::Success();
}

void InspectorPreloadAgent::EnableInternal() {
  DCHECK(GetFrontend());

  enabled_.Set(true);
  instrumenting_agents_->AddInspectorPreloadAgent(this);
}

}  // namespace blink
