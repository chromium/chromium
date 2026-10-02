// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_

#import <memory>
#import <optional>
#import <string_view>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/web_state_id.h"

class GURL;

@class NSError;
namespace web {
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

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UTIL_ACTOR_TEST_UTILS_H_
