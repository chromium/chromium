// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_scene_agent.h"

#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Key used to store the total number of unique days that the user has
// started a session.
NSString* const kTotalDaysOnChrome = @"TotalDaysOnChrome";

// Key used to store an array of unique days that the user has started
// a session in the past 7 days.
NSString* const kActiveDaysInPastWeek = @"ActiveDaysInPastWeek";
}  // namespace

@interface AppStoreRatingSceneAgent ()

// Determines whether the user has used Chrome for at least 3
// different days within the past 7 days.
@property(nonatomic, assign, readonly, getter=isChromeUsed3DaysInPastWeek)
    BOOL chromeUsed3DaysInPastWeek;

// Determines whether the user has used Chrome for at least 15
// different days overall.
@property(nonatomic, assign, readonly, getter=isChromeUsed15Days)
    BOOL chromeUsed15Days;

// Determines whether the user has enabled the Credentials
// Provider Extension.
@property(nonatomic, assign, readonly, getter=isCPEEnabled) BOOL CPEEnabled;

@end

@implementation AppStoreRatingSceneAgent

- (instancetype)init {
  self = [super init];

  return self;
}

- (BOOL)isUserEngaged {
  return IsChromeLikelyDefaultBrowser() && self.chromeUsed3DaysInPastWeek &&
         self.chromeUsed15Days && self.CPEEnabled;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
}

#pragma mark - Getters

- (BOOL)isChromeUsed3DaysInPastWeek {
  if (![[NSUserDefaults standardUserDefaults]
          objectForKey:kActiveDaysInPastWeek]) {
    return NO;
  }
  return [[[NSUserDefaults standardUserDefaults]
             objectForKey:kActiveDaysInPastWeek] count] >= 3 ? YES : NO;
}

- (BOOL)isChromeUsed15Days {
  if (![[NSUserDefaults standardUserDefaults]
          integerForKey:kTotalDaysOnChrome]) {
    return NO;
  }
  return [[NSUserDefaults standardUserDefaults]
             integerForKey:kTotalDaysOnChrome] >= 15 ? YES : NO;
}

- (BOOL)isCPEEnabled {
  DCHECK(self.sceneState.interfaceProvider.mainInterface.browser);
  PrefService* pref_service =
      self.sceneState.interfaceProvider.mainInterface.browser->GetBrowserState()
          ->GetPrefs();
  return password_manager_util::IsCredentialProviderEnabledOnStartup(
      pref_service);
}

#pragma mark - Private

// Calls the PromosManager to request iOS displays the
// App Store Rating prompt to the user.
- (void)requestPromoDisplay {
}

@end
