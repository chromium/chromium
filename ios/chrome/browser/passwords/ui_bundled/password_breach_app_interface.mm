// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_breach_app_interface.h"

#import "ios/chrome/browser/shared/public/commands/password_breach_commands.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation PasswordBreachAppInterface

+ (void)showPasswordBreachWithCheckButton:(BOOL)checkButtonPresent {
  auto handler = chrome_test_util::HandlerForActiveBrowser();
  auto leakType = password_manager::CreateLeakType(
      password_manager::IsSaved(true),
      password_manager::IsReused(checkButtonPresent),
      password_manager::IsSyncing(true));
  [(id<PasswordBreachCommands>)handler showPasswordBreachForLeakType:leakType];
}

@end
