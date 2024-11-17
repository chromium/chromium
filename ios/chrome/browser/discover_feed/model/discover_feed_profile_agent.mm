// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_profile_agent.h"

#import <vector>

#import "base/check.h"
#import "components/push_notification/push_notification_client_id.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_app_agent.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_profile_helper.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace {

// Returns whether the default search engine is the Google Search Engine.
bool IsGoogleDefaultSearchEngine(ProfileIOS* profile) {
  const TemplateURL* const default_search_url_template =
      ios::TemplateURLServiceFactory::GetForProfile(profile)
          ->GetDefaultSearchProvider();

  if (!default_search_url_template) {
    return false;
  }

  return default_search_url_template->prepopulate_id() ==
         TemplateURLPrepopulateData::google.id;
}

}  // namespace

@interface DiscoverFeedProfileAgent () <DiscoverFeedProfileHelper>
@end

@implementation DiscoverFeedProfileAgent

#pragma mark ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  CHECK_EQ(profileState, self.profileState);
  if (nextInitStage != ProfileInitStage::kPrepareUI) {
    return;
  }

  // When the ProfileState reaches InitStagePrepareUI, the -profile property
  // must be set and non-null (i.e. the ProfileIOS must have been loaded).
  ProfileIOS* profile = self.profileState.profile;
  CHECK(profile);

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  const bool isUserSignedIn =
      authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);

  if (isUserSignedIn) {
    if (IsWebChannelsEnabled() && IsDiscoverFeedServiceCreatedEarly()) {
      // Creates the DiscoverFeedService early if the user is signed-in as it is
      // required to follow web channels (and thus is required to interact with
      // any tabs, not just the NTP).
      DiscoverFeedServiceFactory::GetForProfile(profile);
    }
  }

  // Only start doing the content notification user eligibility check if
  // the content notification experiment is enabled.
  if (IsContentNotificationExperimentEnabled() &&
      IsContentNotificationProvisionalEnabled(
          isUserSignedIn, IsGoogleDefaultSearchEngine(profile),
          profile->GetPrefs())) {
    std::vector<PushNotificationClientId> clientIds = {
        PushNotificationClientId::kContent,
        PushNotificationClientId::kSports,
    };

    // This method does not show an UI prompt to the user as provisional
    // notifications are authorized without any user input if the user hasn't
    // previously disabled notifications.
    [ProvisionalPushNotificationUtil
        enrollUserToProvisionalNotificationsForClientIds:std::move(clientIds)
                             clientEnabledForProvisional:YES
                                         withAuthService:authService
                                   deviceInfoSyncService:nil];
  }

  // Register self as a profile helper with the DiscoverFeedAddAgent.
  [[DiscoverFeedAppAgent agentFromApp:self.profileState.appState]
      addHelper:self];
}

#pragma mark DiscoverFeedProfileHelper

- (void)refreshFeedInBackground {
  CHECK(self.profileState.profile);
  if (DiscoverFeedService* service =
          DiscoverFeedServiceFactory::GetForProfileIfExists(
              self.profileState.profile)) {
    service->RefreshFeed(FeedRefreshTrigger::kForegroundAppClose);
  }
}

- (void)performBackgroundRefreshes:(base::OnceCallback<void(bool)>)callback {
  CHECK(self.profileState.profile);
  DiscoverFeedServiceFactory::GetForProfile(self.profileState.profile)
      ->PerformBackgroundRefreshes(base::CallbackToBlock(std::move(callback)));
}

- (void)handleBackgroundRefreshTaskExpiration {
  CHECK(self.profileState.profile);
  DiscoverFeedServiceFactory::GetForProfile(self.profileState.profile)
      ->HandleBackgroundRefreshTaskExpiration();
}

- (base::Time)earliestBackgroundRefreshDate {
  CHECK(self.profileState.profile);
  NSDate* date =
      DiscoverFeedServiceFactory::GetForProfile(self.profileState.profile)
          ->GetEarliestBackgroundRefreshBeginDate();

  return date ? base::Time::FromNSDate(date) : base::Time();
}

@end
