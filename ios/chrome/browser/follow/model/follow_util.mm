// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_util.h"

#import <UIKit/UIKit.h>

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {
// Set the Follow IPH apperance threshold to 15 minutes.
NSTimeInterval const kFollowIPHAppearanceThresholdInSeconds = 15 * 60;
// Set the Follow IPH apperance threshold for site to 1 day.
NSTimeInterval const kFollowIPHAppearanceThresholdForSiteInSeconds =
    24 * 60 * 60;
}  // namespace

NSString* const kFollowIPHPreviousDisplayEvents =
    @"FollowIPHPreviousDisplayEvents";
NSString* const kFollowIPHHost = @"host";
NSString* const kFollowIPHDate = @"date";

FollowActionState GetFollowActionState(web::WebState* webState) {
  // TODO(crbug.com/40251372): Hide Follow action in safe mode.

  if (!webState || !IsWebChannelsEnabled() || IsFeedAblationEnabled()) {
    return FollowActionStateHidden;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(webState->GetBrowserState());

  // Don't show follow option when feed is hidden due to DSE choice.
  if (ShouldHideFeedWithSearchChoice(
          ios::TemplateURLServiceFactory::GetForProfile(profile))) {
    return FollowActionStateHidden;
  }

  // Don't show follow option when following feed is disabled by enterprise
  // policy.
  if (!profile->GetPrefs()->GetBoolean(prefs::kNTPContentSuggestionsEnabled)) {
    return FollowActionStateHidden;
  }

  // Don't show follow option if the user is not signed in.
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);

  // Hide the follow action when users are in incognito mode or when users have
  // not signed in.
  if (profile->IsOffTheRecord() || !authenticationService ||
      !authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    return FollowActionStateHidden;
  }

  const GURL& URL = webState->GetLastCommittedURL();
  // Show the follow action when:
  // 1. The page url is valid;
  // 2. Users are not on NTP or Chrome internal pages;
  if (URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL)) {
    DCHECK(!profile->IsOffTheRecord());
    return FollowActionStateEnabled;
  }
  return FollowActionStateHidden;
}

#pragma mark - For Follow IPH
bool IsFollowIPHShownFrequencyEligible(NSString* host) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  NSArray<NSDictionary*>* followIPHPreviousDisplayEvents =
      [defaults objectForKey:kFollowIPHPreviousDisplayEvents];
  NSDate* lastIPHDate;
  for (NSDictionary* event in followIPHPreviousDisplayEvents) {
    if ([[event objectForKey:kFollowIPHHost] isEqualToString:host]) {
      lastIPHDate = [event objectForKey:kFollowIPHDate];
      break;
    }
  }

  // Return false if its too soon to show another IPH.
  if (lastIPHDate &&
      [[NSDate
          dateWithTimeIntervalSinceNow:-kFollowIPHAppearanceThresholdInSeconds]
          compare:lastIPHDate] == NSOrderedAscending) {
    return false;
  }

  // Return true if it is long enough to show another IPH for this specific
  // site.
  if (!lastIPHDate ||
      [[NSDate dateWithTimeIntervalSinceNow:
                   -kFollowIPHAppearanceThresholdForSiteInSeconds]
          compare:lastIPHDate] != NSOrderedAscending) {
    return true;
  }

  return false;
}

void StoreFollowIPHDisplayEvent(NSString* host) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSArray<NSDictionary*>* followIPHPreviousDisplayEvents =
      [defaults objectForKey:kFollowIPHPreviousDisplayEvents];

  NSMutableArray<NSDictionary*>* updatedDisplayEvents =
      [[NSMutableArray alloc] init];

  // Add object to the the recentEvents while cleaning up dictionary that has a
  // date older than 1 day ago. Since the follow iph will show in a specific
  // regularity, this clean up logic will not execute often
  NSDate* uselessDate =
      [NSDate dateWithTimeIntervalSinceNow:
                  -kFollowIPHAppearanceThresholdForSiteInSeconds];

  for (NSDictionary* event in followIPHPreviousDisplayEvents) {
    if ([[event objectForKey:kFollowIPHDate] compare:uselessDate] !=
        NSOrderedAscending) {
      [updatedDisplayEvents addObject:event];
    }
  }

  // Add the last follow IPH event.
  NSDictionary* IPHPresentingEvent =
      @{kFollowIPHHost : host, kFollowIPHDate : [NSDate date]};
  [updatedDisplayEvents addObject:IPHPresentingEvent];

  [defaults setObject:updatedDisplayEvents
               forKey:kFollowIPHPreviousDisplayEvents];
  [defaults synchronize];
}

void RemoveLastFollowIPHDisplayEvent() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSArray<NSDictionary*>* followIPHPreviousDisplayEvents =
      [defaults objectForKey:kFollowIPHPreviousDisplayEvents];

  DCHECK(followIPHPreviousDisplayEvents);

  NSMutableArray<NSDictionary*>* updatedDisplayEvents =
      [followIPHPreviousDisplayEvents mutableCopy];
  [updatedDisplayEvents removeLastObject];

  [defaults setObject:updatedDisplayEvents
               forKey:kFollowIPHPreviousDisplayEvents];
  [defaults synchronize];
}
