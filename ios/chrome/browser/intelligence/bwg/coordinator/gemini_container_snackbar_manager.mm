// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_snackbar_manager.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

@implementation GeminiContainerSnackbarManager {
  // Browser associated with the container, used to build identity snackbars.
  raw_ptr<Browser> _browser;
  // Authentication service used to retrieve the primary identity.
  raw_ptr<AuthenticationService> _authService;
}

- (instancetype)initWithBrowser:(Browser*)browser
          authenticationService:(AuthenticationService*)authenticationService {
  CHECK(browser);
  CHECK(authenticationService);
  self = [super init];
  if (self) {
    _browser = browser;
    _authService = authenticationService;
  }
  return self;
}

#pragma mark - Public

- (void)showSnackbarIfAccountMismatchedForStartupState:
    (GeminiStartupState*)startupState {
  if (!IsIOSGeminiBottomSheetMigrationEnabled() ||
      !IsAppSwitcherAISummarizationEnabled() ||
      !startupState.isMismatchedAccount || !self.snackbarHandler || !_browser ||
      !_authService) {
    return;
  }

  id<SystemIdentity> identity = _authService->GetPrimaryIdentity();
  if (!identity) {
    return;
  }

  SnackbarMessage* message =
      CreateIdentitySnackbarMessage(identity, _browser.get());
  if (!message) {
    return;
  }

  [self.snackbarHandler showSnackbarMessage:message];
}

- (void)disconnect {
  self.snackbarHandler = nil;
  _browser = nullptr;
  _authService = nullptr;
}

@end
