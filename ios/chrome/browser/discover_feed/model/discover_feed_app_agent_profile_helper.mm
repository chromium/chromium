// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_app_agent_profile_helper.h"

#import "base/check.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
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

@interface DiscoverFeedAppAgentProfileHelper () <ProfileStateObserver>

@end

@implementation DiscoverFeedAppAgentProfileHelper {
  ProfileState* _profileState;
}

- (instancetype)initWithProfileState:(ProfileState*)profileState {
  if ((self = [super init])) {
    CHECK(profileState);
    _profileState = profileState;
    [_profileState addObserver:self];
  }
  return self;
}

- (void)refreshFeedInBackground {
  // The DiscoverFeedAppAgent does not track the ProfileState -initState,
  // so it may call this method before the profile is fully initialized.
  // In that case, ignore the call.
  if (_profileState.initStage < ProfileInitStage::kPrepareUI) {
    return;
  }

  // The profile must be loaded (and thus -profile non null) when the
  // ProfileState has reached ProfileInitStage::kPrepareUI.
  ProfileIOS* profile = _profileState.profile;
  CHECK(profile);

  if (DiscoverFeedService* service =
          DiscoverFeedServiceFactory::GetForProfileIfExists(profile)) {
    service->RefreshFeed(FeedRefreshTrigger::kForegroundAppClose);
  }
}

- (void)shutdown {
  // Safe to call even if the object no longer observe the ProfileState.
  [_profileState removeObserver:self];
  _profileState = nil;
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  CHECK_EQ(profileState, _profileState);
  if (nextInitStage < ProfileInitStage::kPrepareUI) {
    return;
  }

  // There is no need to observe the ProfileState anymore.
  [_profileState removeObserver:self];

  // The profile must be loaded (and thus -profile non null) when the
  // ProfileState has reached ProfileInitStage::kPrepareUI.
  ProfileIOS* profile = _profileState.profile;
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
    // This method does not show an UI prompt to the user as provisional
    // notifications are authorized without any user input if the user hasn't
    // previously disabled notifications.
    ProvisionalPushNotificationServiceFactory::GetForProfile(profile)
        ->EnrollUserToProvisionalNotifications(
            ProvisionalPushNotificationService::ClientIdState::kEnabled,
            {
                PushNotificationClientId::kContent,
                PushNotificationClientId::kSports,
            });
  }
}

@end
