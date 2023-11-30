// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/action.h"

#import <cstring>
#import <utility>
#import <vector>

#import "base/callback_list.h"
#import "base/check_is_test.h"
#import "base/containers/flat_map.h"
#import "base/containers/flat_set.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/ranges/algorithm.h"
#import "base/scoped_observation.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_observer.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace enterprise_idle {

namespace {

// Action that closes all regular and incognito tabs for the profile.
class CloseTabsAction : public Action {
 public:
  CloseTabsAction() : Action(static_cast<int>(ActionType::kCloseTabs)) {}

  // Action:
  void Run(ChromeBrowserState* browser_state,
           Continuation continuation) override {
    BrowserList* browser_list =
        BrowserListFactory::GetForBrowserState(browser_state);
    for (Browser* browser : browser_list->AllIncognitoBrowsers()) {
      browser->GetWebStateList()->CloseAllWebStates(
          WebStateList::CLOSE_NO_FLAGS);
    }
    for (Browser* browser : browser_list->AllRegularBrowsers()) {
      browser->GetWebStateList()->CloseAllWebStates(
          WebStateList::CLOSE_NO_FLAGS);
    }
    std::move(continuation).Run(true);
  }
};

class SignOutAction : public Action {
 public:
  SignOutAction() : Action(static_cast<int>(ActionType::kSignOut)) {}

  // Action:
  void Run(ChromeBrowserState* browser_state,
           Continuation continuation) override {
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state);
    if (authentication_service->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin)) {
      authentication_service->SignOut(
          signin_metrics::ProfileSignout::kIdleTimeoutPolicyTriggeredSignOut,
          /*force_clear_browsing_data=*/false,
          base::CallbackToBlock(base::BindOnce(std::move(continuation), true)));
      return;
    }
    // Run continuation right away if user is not signed in.
    std::move(continuation).Run(true);
  }
};

// Action that clears one or more types of data via BrowsingDataRemover.
// Multiple data types may be grouped into a single ClearBrowsingDataAction
// object.
class ClearBrowsingDataAction : public Action,
                                public BrowsingDataRemoverObserver {
 public:
  explicit ClearBrowsingDataAction(
      base::flat_set<ActionType> action_types,
      BrowsingDataRemover* main_browsing_data_remover,
      BrowsingDataRemover* incognito_browsing_data_remover)
      : Action(static_cast<int>(ActionType::kClearBrowsingHistory)),
        action_types_(action_types),
        main_browsing_data_remover_(main_browsing_data_remover),
        incognito_browsing_data_remover_(incognito_browsing_data_remover) {}

  ~ClearBrowsingDataAction() override = default;

  // Action:
  void Run(ChromeBrowserState* browser_state,
           Continuation continuation) override {
    continuation_ = std::move(continuation);
    mask_ = GetRemoveMask();

    if (IsRemoveDataMaskSet(mask_, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
      // If browsing History will be cleared set the kLastClearBrowsingDataTime.
      // TODO(crbug.com/1085419): This pref is used by the Feed to prevent the
      // showing of customized content after history has been cleared.
      browser_state->GetPrefs()->SetInt64(
          browsing_data::prefs::kLastClearBrowsingDataTime,
          base::Time::Now().ToTimeT());
      DiscoverFeedServiceFactory::GetForBrowserState(browser_state)
          ->BrowsingHistoryCleared();
    }

    ClearDataForBrowserState(browser_state);
  }

  // BrowsingDataRemoverObserver:
  void OnBrowsingDataRemoved(BrowsingDataRemover* remover,
                             BrowsingDataRemoveMask mask) override {
    // Clearing action fails if clearing for either incognito or main browser
    // fails.
    removal_sucess_ = (mask == mask_) && removal_sucess_;
    removals_completed_count_++;
    // Run `continutation_` only if both removers are done removing.
    if (!main_browsing_data_remover_->IsRemoving() &&
        !incognito_browsing_data_remover_->IsRemoving() &&
        removals_completed_count_ == 2) {
      main_scoped_observer_.Reset();
      incognito_scoped_observer_.Reset();
      std::move(continuation_).Run(removal_sucess_);
    }
  }

 private:
  // TODO(b/301676922): make sure to set and unset the scenes'
  // userInteractionEnabled before and after calling run actions respectively if
  // remove site data is to be cleared.
  void ClearDataForBrowserState(ChromeBrowserState* browser_state) {
    incognito_scoped_observer_.Observe(incognito_browsing_data_remover_);
    incognito_browsing_data_remover_->Remove(
        browsing_data::TimePeriod::ALL_TIME, mask_, {});

    main_scoped_observer_.Observe(main_browsing_data_remover_);
    main_browsing_data_remover_->Remove(browsing_data::TimePeriod::ALL_TIME,
                                        mask_, {});
  }

  BrowsingDataRemoveMask GetRemoveMask() const {
    static const std::pair<ActionType, BrowsingDataRemoveMask> entries[] = {
        {ActionType::kClearBrowsingHistory,
         BrowsingDataRemoveMask::REMOVE_HISTORY},
        {ActionType::kClearCookiesAndOtherSiteData,
         BrowsingDataRemoveMask::REMOVE_SITE_DATA},
        {ActionType::kClearCachedImagesAndFiles,
         BrowsingDataRemoveMask::REMOVE_CACHE},
        {ActionType::kClearPasswordSignin,
         BrowsingDataRemoveMask::REMOVE_PASSWORDS},
        {ActionType::kClearAutofill, BrowsingDataRemoveMask::REMOVE_FORM_DATA}};
    BrowsingDataRemoveMask result = BrowsingDataRemoveMask::REMOVE_NOTHING;
    for (const auto& [action_type, mask] : entries) {
      if (base::Contains(action_types_, action_type)) {
        result |= mask;
      }
    }
    return result;
  }

  base::flat_set<ActionType> action_types_;
  base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemoverObserver>
      main_scoped_observer_{this};
  base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemoverObserver>
      incognito_scoped_observer_{this};
  BrowsingDataRemover* main_browsing_data_remover_;
  BrowsingDataRemover* incognito_browsing_data_remover_;
  Continuation continuation_;
  // Removal mask defined by the clear actions set in the IdleTimeoutActions
  // policy list.
  BrowsingDataRemoveMask mask_;
  // Both main and incognito browser data should be deleted, so this count
  // should reach 2 for action completion. This is a guard in case one browser
  // data removal completed before the other one starts running.
  int removals_completed_count_ = 0;
  bool removal_sucess_{true};
};

}  // namespace

Action::Action(int priority) : priority_(priority) {}

Action::~Action() = default;

bool ActionFactory::CompareActionsByPriority::operator()(
    const std::unique_ptr<Action>& a,
    const std::unique_ptr<Action>& b) const {
  return a->priority() > b->priority();
}

ActionFactory::ActionQueue ActionFactory::Build(
    const std::vector<ActionType>& action_types,
    BrowsingDataRemover* main_browsing_data_remover,
    BrowsingDataRemover* incognito_browsing_data_remover) {
  std::vector<std::unique_ptr<Action>> actions;

  base::flat_set<ActionType> clear_actions;
  for (auto action_type : action_types) {
    switch (action_type) {
      case ActionType::kCloseTabs:
        actions.push_back(std::make_unique<CloseTabsAction>());
        break;
      case ActionType::kSignOut:
        actions.push_back(std::make_unique<SignOutAction>());
        break;

      // "clear_*" actions are all grouped into a single Action object. Collect
      // them in a flat_set<>, and create the shared action object once we have
      // the set.
      case ActionType::kClearBrowsingHistory:
      case ActionType::kClearCookiesAndOtherSiteData:
      case ActionType::kClearCachedImagesAndFiles:
      case ActionType::kClearPasswordSignin:
      case ActionType::kClearAutofill:
        clear_actions.insert(action_type);
        break;
      default:
        // Perform validation in the `PolicyHandler` if a new type is added.
        NOTREACHED();
    }
  }

  if (!clear_actions.empty()) {
    // Merge "clear_*" actions into a single Action.
    actions.push_back(std::make_unique<ClearBrowsingDataAction>(
        std::move(clear_actions), main_browsing_data_remover,
        incognito_browsing_data_remover));
  }

  return ActionQueue(ActionQueue::value_compare(), std::move(actions));
}

ActionFactory::ActionFactory() = default;
ActionFactory::~ActionFactory() = default;

}  // namespace enterprise_idle
