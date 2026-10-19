// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_

#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"

class GURL;

@class NSError;
namespace web {
class FakeNavigationContext;
class FakeWebState;
}  // namespace web

namespace actor {

// A `FakeNavigationManager` that automatically simulates navigation start and
// completion on `web_state` when `LoadURLWithParams()`, `GoBack()`, or
// `GoForward()` is called.

class CompletingFakeNavigationManager : public web::FakeNavigationManager {
 public:
  explicit CompletingFakeNavigationManager(web::FakeWebState* web_state);
  ~CompletingFakeNavigationManager() override;
  // `web::FakeNavigationManager` overrides.
  void LoadURLWithParams(
      const NavigationManager::WebLoadParams& params) override;
  void GoBack() override;
  void GoForward() override;
  // Configures whether navigations automatically fire `OnNavigationStarted` and
  // `OnNavigationFinished`.
  void set_auto_complete(bool auto_complete) { auto_complete_ = auto_complete; }
  // Configures whether simulated navigations commit (`HasCommitted()`).
  void set_has_committed(bool has_committed) { has_committed_ = has_committed; }
  // Configures an optional error for simulated navigations.
  void set_error(NSError* error) { error_ = error; }

 private:
  void SimulateNavigation(const GURL& url);
  raw_ptr<web::FakeWebState> web_state_ = nullptr;
  bool auto_complete_ = true;
  bool has_committed_ = true;
  NSError* error_ = nil;
};

// A `FakeNavigationManager` that simulates the full navigation lifecycle
// including `ShouldAllowResponse` policy evaluation before finishing the
// navigation.
class ResponseSimulatingNavigationManager : public web::FakeNavigationManager {
 public:
  explicit ResponseSimulatingNavigationManager(web::FakeWebState* web_state);
  ~ResponseSimulatingNavigationManager() override;

  // `web::FakeNavigationManager` overrides.
  void LoadURLWithParams(
      const NavigationManager::WebLoadParams& params) override;

  // Returns the last `PolicyDecision` produced by `ShouldAllowResponse`.
  const std::optional<web::WebStatePolicyDecider::PolicyDecision>&
  last_response_decision() const {
    return last_response_decision_;
  }

 private:
  void OnResponseDecision(std::unique_ptr<web::FakeNavigationContext> context,
                          web::WebStatePolicyDecider::PolicyDecision decision);

  raw_ptr<web::FakeWebState> web_state_ = nullptr;
  std::optional<web::WebStatePolicyDecider::PolicyDecision>
      last_response_decision_;
  base::WeakPtrFactory<ResponseSimulatingNavigationManager> weak_ptr_factory_{
      this};
};

// Fake `OriginGatingChecker::Delegate` that returns a fixed decision
// (`is_allowed`) for testing.
class FakeOriginGatingCheckerDelegate
    : public origin_gating::OriginGatingChecker::Delegate {
 public:
  explicit FakeOriginGatingCheckerDelegate(bool is_allowed);
  ~FakeOriginGatingCheckerDelegate() override;

  // `origin_gating::OriginGatingChecker::Delegate` implementation.
  void DoesOriginRequireUserConfirmation(
      origin_gating::GatingDecisionContext* context,
      const origin_gating::GateableEvent& event,
      DoesOriginRequireUserConfirmationCallback callback) const override;
  void EvaluateEnterprisePolicy(
      const GURL& destination,
      EvaluateEnterprisePolicyCallback callback) const override;
  void OnNoVerdict(origin_gating::GatingDecisionContext* context,
                   const origin_gating::GateableEvent& event,
                   bool requires_user_confirmation,
                   base::OnceCallback<void(NoVerdictResult)> callback) override;

  // Returns a `WeakPtr` to this delegate instance.
  base::WeakPtr<FakeOriginGatingCheckerDelegate> GetWeakPtr();

 private:
  const bool is_allowed_;
  base::WeakPtrFactory<FakeOriginGatingCheckerDelegate> weak_ptr_factory_{this};
};

// Creates a successful tool request. Selects the `Wait` action arbitrarily
// as a representative successful action, optionally targeting `identifier`.
std::unique_ptr<ActorToolRequest> MakeSuccessfulActorToolRequest(
    web::WebStateID identifier = web::WebStateID());

// Creates an Action proto that will succeed.
optimization_guide::proto::Action MakeSuccessfulActorAction(
    web::WebStateID identifier = web::WebStateID());

// Creates a tool request that fails execution. Selects the `Click` action
// arbitrarily because it fails execution when no target tab is registered.
std::unique_ptr<ActorToolRequest> MakeFailingActorToolRequest();

// Creates an Action proto that will fail execution.
optimization_guide::proto::Action MakeFailingActorAction();

// Returns true if `journal` contains an entry matching `event_name` and, if
// specified, `entry_type`.
bool HasJournalEntry(
    const AggregatedJournal& journal,
    std::string_view event_name,
    std::optional<mojom::JournalEntryType> entry_type = std::nullopt);

// Returns true if `journal` contains an entry matching `event_name` (and, if
// specified, `entry_type`) with a detail entry matching `detail_key` and
// `detail_value`.
bool HasJournalEntryWithDetail(
    const AggregatedJournal& journal,
    std::string_view event_name,
    std::string_view detail_key,
    std::string_view detail_value,
    std::optional<mojom::JournalEntryType> entry_type = std::nullopt);

// Records a copy of every entry added to an `AggregatedJournal` while it is
// alive. Unlike `AggregatedJournal::Items()`, which only keeps the most recent
// entries, no entry is dropped, so that tests can verify the order and nesting
// of long sequences of entries.
class RecordingJournalObserver : public AggregatedJournal::Observer {
 public:
  explicit RecordingJournalObserver(AggregatedJournal& journal);
  RecordingJournalObserver(const RecordingJournalObserver&) = delete;
  RecordingJournalObserver& operator=(const RecordingJournalObserver&) = delete;
  ~RecordingJournalObserver() override;

  // `AggregatedJournal::Observer` implementation.
  void WillAddJournalEntry(const AggregatedJournal::Entry& entry) override;

  // Returns all the recorded entries, in the order they were added.
  const std::vector<mojom::JournalEntryPtr>& entries() const {
    return entries_;
  }

  // Returns the recorded entries named `event_name`, in the order they were
  // added.
  std::vector<const mojom::JournalEntry*> EntriesNamed(
      std::string_view event_name) const;

 private:
  std::vector<mojom::JournalEntryPtr> entries_;
  base::ScopedObservation<AggregatedJournal, AggregatedJournal::Observer>
      observation_{this};
};

// Returns the value of the detail of `entry` keyed `key`, if any. The value is
// copied so that it stays valid after `entry` is destroyed.
std::optional<std::string> FindJournalDetail(const mojom::JournalEntry& entry,
                                             std::string_view key);

// Returns success if, on every track, each `kEnd` entry closes the most
// recently opened and still open `kBegin` entry, which must have the same name,
// and no slice is left open after the last entry. This mirrors how Perfetto
// builds slices, so a failure means that the trace would contain misplaced end
// events or slices that never end.
testing::AssertionResult IsStrictlyNestedPerTrack(
    const std::vector<mojom::JournalEntryPtr>& entries);

// Runs the tasks posted to the current sequence, including the tasks they post
// in turn, until it is idle. Never advances mock time or waits for delayed
// tasks or other threads, so it is deterministic for single-sequence code.
void FlushCurrentSequence();

// Runs only the tasks already posted to the current sequence, not the tasks
// they post in turn, e.g. to observe an intermediate state. Same guarantees as
// `FlushCurrentSequence()`.
void RunAlreadyPostedTasks();

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_
