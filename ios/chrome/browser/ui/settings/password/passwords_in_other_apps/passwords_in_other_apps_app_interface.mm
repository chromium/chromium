// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_app_interface.h"

#import "ios/chrome/test/fakes/fake_password_auto_fill_status_manager.h"

@implementation PasswordsInOtherAppsAppInterface

+ (FakePasswordAutoFillStatusManager*)manager {
  return [FakePasswordAutoFillStatusManager sharedFakeManager];
}

#pragma mark - Swizzling

+ (id)swizzlePasswordAutoFillStatusManagerWithFake {
  FakePasswordAutoFillStatusManager* (^swizzlePasswordAutoFillManagerBlock)(
      void) = ^{
    return [self manager];
  };
  return swizzlePasswordAutoFillManagerBlock;
}

#pragma mark - Mocking and Expectations

+ (void)startFakeManagerWithAutoFillStatus:(BOOL)autoFillEnabled {
  [[self manager] startFakeManagerWithAutoFillStatus:autoFillEnabled];
}

+ (void)setAutoFillStatus:(BOOL)autoFillEnabled {
  [[self manager] setAutoFillStatus:autoFillEnabled];
}

+ (void)resetManager {
  [[self manager] reset];
}

@end
