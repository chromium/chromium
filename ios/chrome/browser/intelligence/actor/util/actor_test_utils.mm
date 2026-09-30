// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"

#import <utility>

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/origin_gating/core/types.h"
#import "url/gurl.h"

namespace actor {

FakeOriginGatingCheckerDelegate::FakeOriginGatingCheckerDelegate(
    bool is_allowed)
    : is_allowed_(is_allowed) {}

FakeOriginGatingCheckerDelegate::~FakeOriginGatingCheckerDelegate() = default;

void FakeOriginGatingCheckerDelegate::DoesOriginRequireUserConfirmation(
    origin_gating::GatingDecisionContext* context,
    const origin_gating::GateableEvent& event,
    DoesOriginRequireUserConfirmationCallback callback) const {
  std::move(callback).Run(false);
}

void FakeOriginGatingCheckerDelegate::EvaluateEnterprisePolicy(
    const GURL& destination,
    EvaluateEnterprisePolicyCallback callback) const {
  std::move(callback).Run({.decision = origin_gating::Decision::kNoDecision});
}

void FakeOriginGatingCheckerDelegate::OnNoVerdict(
    origin_gating::GatingDecisionContext* context,
    const origin_gating::GateableEvent& event,
    bool requires_user_confirmation,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  std::move(callback).Run({.is_allowed = is_allowed_,
                           .did_prompt_user = false,
                           .bypass_cache = true});
}

base::WeakPtr<FakeOriginGatingCheckerDelegate>
FakeOriginGatingCheckerDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<ActorToolRequest> MakeSuccessfulActorToolRequest(
    web::WebStateID identifier) {
  return std::make_unique<ActorToolRequest>(
      MakeSuccessfulActorAction(identifier));
}

optimization_guide::proto::Action MakeSuccessfulActorAction(
    web::WebStateID identifier) {
  optimization_guide::proto::Action action;
  auto* wait = action.mutable_wait();
  wait->set_wait_time_ms(0);
  if (identifier.valid()) {
    wait->set_observe_tab_id(identifier.identifier());
  }
  return action;
}

std::unique_ptr<ActorToolRequest> MakeFailingActorToolRequest() {
  return std::make_unique<ActorToolRequest>(MakeFailingActorAction());
}

optimization_guide::proto::Action MakeFailingActorAction() {
  // This proto will fail initial validation when a tab_id is not set.
  optimization_guide::proto::Action action;
  action.mutable_click();
  return action;
}

namespace {

// Returns true if `journal` contains an entry matching `event_name`,
// `entry_type` (if specified), and optional `detail_key`/`detail_value` filter.
bool HasMatchingJournalEntry(
    const AggregatedJournal& journal,
    std::string_view event_name,
    std::optional<mojom::JournalEntryType> entry_type,
    std::optional<std::string_view> detail_key = std::nullopt,
    std::optional<std::string_view> detail_value = std::nullopt) {
  for (auto it = journal.Items(); it; ++it) {
    const std::unique_ptr<AggregatedJournal::Entry>* entry_ptr = *it;
    if (!entry_ptr || !*entry_ptr || !(*entry_ptr)->data) {
      continue;
    }
    const AggregatedJournal::Entry* entry = entry_ptr->get();
    if (entry->data->event != event_name) {
      continue;
    }
    if (entry_type.has_value() && entry->data->type != *entry_type) {
      continue;
    }
    if (detail_key.has_value() && detail_value.has_value()) {
      bool found_detail = false;
      for (const auto& detail : entry->data->details) {
        if (detail && detail->key == *detail_key &&
            detail->value == *detail_value) {
          found_detail = true;
          break;
        }
      }
      if (!found_detail) {
        continue;
      }
    }
    return true;
  }
  return false;
}

}  // namespace

bool HasJournalEntry(const AggregatedJournal& journal,
                     std::string_view event_name,
                     std::optional<mojom::JournalEntryType> entry_type) {
  return HasMatchingJournalEntry(journal, event_name, entry_type);
}

bool HasJournalEntryWithDetail(
    const AggregatedJournal& journal,
    std::string_view event_name,
    std::string_view detail_key,
    std::string_view detail_value,
    std::optional<mojom::JournalEntryType> entry_type) {
  return HasMatchingJournalEntry(journal, event_name, entry_type, detail_key,
                                 detail_value);
}

}  // namespace actor
