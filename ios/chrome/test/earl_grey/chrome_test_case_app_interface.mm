// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_test_case_app_interface.h"

#import "base/check.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"

namespace {

// Stores the completion UUIDs when the completion is invoked. The UUIDs can be
// checked with +[ChromeTestCaseAppInterface isCompletionInvokedWithUUID:].
NSMutableSet* invokedCompletionUUID = nil;
}

@implementation ChromeTestCaseAppInterface

+ (void)setUpMockAuthentication {
  chrome_test_util::SetUpMockAuthentication();
}

+ (void)tearDownMockAuthentication {
  chrome_test_util::TearDownMockAuthentication();
}

+ (void)resetAuthentication {
  chrome_test_util::ResetSigninPromoPreferences();
  chrome_test_util::ResetMockAuthentication();
  chrome_test_util::ResetSyncAccountSettingsPrefs();
  chrome_test_util::ResetHistorySyncPreferencesForTesting();
}

+ (void)removeInfoBarsAndPresentedStateWithCompletionUUID:
    (NSUUID*)completionUUID {
  chrome_test_util::RemoveAllInfoBars();
  chrome_test_util::ClearPresentedState(^() {
    if (completionUUID)
      [self completionInvokedWithUUID:completionUUID];
  });
}

+ (void)blockSigninIPH {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  tracker->NotifyUsedEvent(
      feature_engagement::kIPHiOSReplaceSyncPromosWithSignInPromos);
}

+ (BOOL)isCompletionInvokedWithUUID:(NSUUID*)completionUUID {
  if (![invokedCompletionUUID containsObject:completionUUID])
    return NO;
  [invokedCompletionUUID removeObject:completionUUID];
  return YES;
}

#pragma mark - Private

+ (void)completionInvokedWithUUID:(NSUUID*)completionUUID {
  if (!invokedCompletionUUID)
    invokedCompletionUUID = [NSMutableSet set];
  DCHECK(![invokedCompletionUUID containsObject:completionUUID]);
  [invokedCompletionUUID addObject:completionUUID];
}

@end
