// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OmniboxPedalAnnotator

- (OmniboxPedalData*)pedalForMatch:(const AutocompleteMatch&)match {
  if (!match.action) {
    return nil;
  }
  __weak id<ApplicationCommands> pedalsEndpoint = self.pedalsEndpoint;

  switch (match.action->GetID()) {
    case (int)OmniboxPedalId::PLAY_CHROME_DINO_GAME: {
      return [[OmniboxPedalData alloc]
          initWithHint:@"Click"
                action:^{
                  OpenNewTabCommand* command = [OpenNewTabCommand
                      commandWithURLFromChrome:GURL("chrome://dino")
                                   inIncognito:NO];
                  [pedalsEndpoint openURLInNewTab:command];
                }];
    }
    case (int)OmniboxPedalId::CLEAR_BROWSING_DATA: {
      return [[OmniboxPedalData alloc]
          initWithHint:@"Click"
                action:^{
                  OpenNewTabCommand* command = [OpenNewTabCommand
                      commandWithURLFromChrome:GURL("chrome://dino")
                                   inIncognito:NO];
                  [pedalsEndpoint openURLInNewTab:command];
                }];
    }
    default:
      return nil;
  }
}

@end
