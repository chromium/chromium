// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/otr_profile_destroyer_profile_agent.h"

#import "base/callback_list.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/sequence_checker.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller+OTRProfileDeletion.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_incognito_session_tracker.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace {

// For readability of the loop in -onBrowsingDataRemoved.
constexpr BrowserList::BrowserType kRegularAndIncognito =
    BrowserList::BrowserType::kRegularAndIncognito;

}  // namespace

@implementation OTRPRofileDestroyerProfileAgent {
  // The owning ProfileState.
  __weak ProfileState* _profileState;

  // Subscription to listen for the profile destruction.
  base::CallbackListSubscription _profileSubscription;

  // Used to track whether the profile has any open off-the-record tabs.
  std::unique_ptr<ProfileIncognitoSessionTracker> _tracker;

  // Ensure sequence-affinity.
  SEQUENCE_CHECKER(_sequenceChecker);
}

#pragma mark - ProfileStateAgent

- (void)setProfileState:(ProfileState*)profileState {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK_GE(profileState.initStage, ProfileInitStage::kProfileLoaded);
  _profileState = profileState;

  ProfileIOS* profile = _profileState.profile;
  CHECK(profile);

  __weak OTRPRofileDestroyerProfileAgent* weakSelf = self;
  _profileSubscription =
      profile->RegisterProfileDestroyedCallback(base::BindOnce(^{
        [weakSelf onProfileDestroyed];
      }));

  _tracker = std::make_unique<ProfileIncognitoSessionTracker>(
      BrowserListFactory::GetForProfile(profile),
      base::BindRepeating(^(bool has_incognito_tabs) {
        [weakSelf onHasIncognitoTabsChanged:has_incognito_tabs];
      }));
}

#pragma mark - Private methods

- (void)onProfileDestroyed {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _profileState = nil;
  _tracker.reset();
}

- (void)onHasIncognitoTabsChanged:(bool)hasIncognitoTabs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!hasIncognitoTabs) {
    // The last incognito tab has been closed, and the off-the-record profile
    // must be destroyed and recreated. Queue an empty task on the IO thread
    // to give a chance for the task in progress to complete before destroying
    // the profile.
    __weak OTRPRofileDestroyerProfileAgent* weakSelf = self;
    web::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), base::BindOnce(^{
          [weakSelf maybeDestroyAndRecreateOTRProfile];
        }));
  }
}

- (void)maybeDestroyAndRecreateOTRProfile {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // It is possible for new incognito tabs to have been opened while waiting
  // for the IO requests to finish. If this is the case, do nothing. Profile
  // will be destroyed the next time the last incognito tab is closed.
  if (_tracker && !_tracker->has_incognito_tabs()) {
    [self destroyAndRecreateOTRProfile];
  }
}

- (void)destroyAndRecreateOTRProfile {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(true);

  ProfileIOS* profile = _profileState.profile;
  CHECK(profile->HasOffTheRecordProfile());

  // Notify all connected scenes that the incognito profile will be destroyed.
  for (SceneState* sceneState in _profileState.connectedScenes) {
    [sceneState.controller willDestroyIncognitoProfile];
  }

  // Clears incognito data that is specific to iOS and won't be cleared by
  // deleting the profile.
  __weak OTRPRofileDestroyerProfileAgent* weakSelf = self;
  BrowsingDataRemoverFactory::GetForProfile(profile->GetOffTheRecordProfile())
      ->Remove(browsing_data::TimePeriod::ALL_TIME,
               BrowsingDataRemoveMask::REMOVE_ALL, base::BindOnce(^{
                 [weakSelf onBrowsingDataRemoved];
               }));

  // Destroy and recreate the incognito profile.
  profile->DestroyOffTheRecordProfile();
  CHECK(!profile->HasOffTheRecordProfile());

  profile->GetOffTheRecordProfile();
  CHECK(profile->HasOffTheRecordProfile());

  // Notify all connected scenes that the incognito profile has been recreated.
  for (SceneState* sceneState in _profileState.connectedScenes) {
    [sceneState.controller incognitoProfileCreated];
  }

  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(false);
}

- (void)onBrowsingDataRemoved {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ProfileIOS* profile = _profileState.profile;
  if (!profile) {
    // If the profile has already been destroyed, there is nothing to do.
    return;
  }

  if (BrowsingDataRemover* remover =
          BrowsingDataRemoverFactory::GetForProfileIfExists(profile);
      remover && remover->IsRemoving()) {
    // If data removing is in progress, then do not enable web usage.
    return;
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  for (Browser* browser : browser_list->BrowsersOfType(kRegularAndIncognito)) {
    WebUsageEnablerBrowserAgent::FromBrowser(browser)->SetWebUsageEnabled(true);
  }
}

@end
