// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"

#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"

ProceduralBlockWithURL BlockToOpenURL(UIResponder* responder,
                                      id<ApplicationCommands> handler) {
  __weak UIResponder* weakResponder = responder;
  __weak id<ApplicationCommands> weakHandler = handler;
  auto blockToOpenURL = ^(const GURL& url) {
    UIResponder* strongResponder = weakResponder;
    if (!strongResponder)
      return;
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:url];
    [weakHandler closeSettingsUIAndOpenURL:command];
  };
  return blockToOpenURL;
}
