// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_app_interface.h"

#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation OmniboxPositionChoiceAppInterface

+ (void)showOmniboxPositionChoiceScreen {
  id<BrowserCoordinatorCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  [handler showOmniboxPositionChoice];
}

@end
