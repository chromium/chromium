// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_test_case_app_interface.h"

#import "base/check.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/signin_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  chrome_test_util::ResetUserApprovedAccountListManager();
}

+ (void)removeInfoBarsAndPresentedStateWithCompletionUUID:
    (NSUUID*)completionUUID {
  chrome_test_util::RemoveAllInfoBars();
  chrome_test_util::ClearPresentedState(^() {
    if (completionUUID)
      [self completionInvokedWithUUID:completionUUID];
  });
}

+ (BOOL)isCompletionInvokedWithUUID:(NSUUID*)completionUUID {
  if (![invokedCompletionUUID containsObject:completionUUID])
    return NO;
  [invokedCompletionUUID removeObject:completionUUID];
  return YES;
}

+ (void)disableKeyboardTutorials {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    // Set the preferences values directly on simulator for the keyboard
    // modifiers. For persisting these values, CFPreferencesSynchronize must be
    // called after.
    CFStringRef app = CFSTR("com.apple.keyboard.preferences.plist");
    CFPreferencesSetValue(CFSTR("DidShowContinuousPathIntroduction"),
                          kCFBooleanTrue, app, kCFPreferencesAnyUser,
                          kCFPreferencesAnyHost);
    CFPreferencesSetValue(CFSTR("KeyboardDidShowProductivityTutorial"),
                          kCFBooleanTrue, app, kCFPreferencesAnyUser,
                          kCFPreferencesAnyHost);
    CFPreferencesSetValue(CFSTR("DidShowGestureKeyboardIntroduction"),
                          kCFBooleanTrue, app, kCFPreferencesAnyUser,
                          kCFPreferencesAnyHost);
    CFPreferencesSetValue(
        CFSTR("UIKeyboardDidShowInternationalInfoIntroduction"), kCFBooleanTrue,
        app, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    CFPreferencesSynchronize(kCFPreferencesAnyApplication,
                             kCFPreferencesAnyUser, kCFPreferencesAnyHost);
  });
}

#pragma mark - Private

+ (void)completionInvokedWithUUID:(NSUUID*)completionUUID {
  if (!invokedCompletionUUID)
    invokedCompletionUUID = [NSMutableSet set];
  DCHECK(![invokedCompletionUUID containsObject:completionUUID]);
  [invokedCompletionUUID addObject:completionUUID];
}

@end
