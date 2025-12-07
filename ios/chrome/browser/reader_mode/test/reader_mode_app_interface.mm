// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/test/reader_mode_app_interface.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/web_state.h"

@implementation ReaderModeAppInterface

+ (bool)readerModeWebStateIsReady {
  Browser* browser = chrome_test_util::GetCurrentBrowser();
  web::WebState* web_state = browser->GetWebStateList()->GetActiveWebState();
  return ReaderModeTabHelper::FromWebState(web_state)
             ->GetReaderModeWebState() != nullptr;
}

+ (void)showReaderMode {
  id<ReaderModeCommands> handler = HandlerForProtocol(
      chrome_test_util::GetCurrentBrowser()->GetCommandDispatcher(),
      ReaderModeCommands);
  [handler showReaderModeFromAccessPoint:ReaderModeAccessPoint::kToolsMenu];
}

+ (void)hideReaderMode {
  id<ReaderModeCommands> handler = HandlerForProtocol(
      chrome_test_util::GetCurrentBrowser()->GetCommandDispatcher(),
      ReaderModeCommands);
  [handler hideReaderMode];
}

@end
