// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"

#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"

@implementation SafetyCheckState

- (instancetype)
    initWithUpdateChromeState:(UpdateChromeSafetyCheckState)updateChromeState
                passwordState:(PasswordSafetyCheckState)passwordState
            safeBrowsingState:(SafeBrowsingSafetyCheckState)safeBrowsingState
                 runningState:(RunningSafetyCheckState)runningState {
  if ((self = [super init])) {
    _updateChromeState = updateChromeState;
    _passwordState = passwordState;
    _safeBrowsingState = safeBrowsingState;
    _runningState = runningState;
  }

  return self;
}

#pragma mark - Public

- (NSUInteger)numberOfIssues {
  NSUInteger invalidCheckCount = 0;
  if (InvalidUpdateChromeState(_updateChromeState)) {
    invalidCheckCount++;
  }
  if (InvalidPasswordState(_passwordState)) {
    invalidCheckCount++;
  }
  if (InvalidSafeBrowsingState(_safeBrowsingState)) {
    invalidCheckCount++;
  }

  return invalidCheckCount;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kSafetyCheck;
}

@end
