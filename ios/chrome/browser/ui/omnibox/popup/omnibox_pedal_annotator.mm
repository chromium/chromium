// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OmniboxPedalAnnotator

- (OmniboxPedalData*)pedalForMatch:(const AutocompleteMatch&)match {
  if (!match.action) {
    return nil;
  }
  __weak id<ApplicationCommands> pedalsEndpoint = self.pedalsEndpoint;
  __weak id<OmniboxCommands> omniboxCommandHandler = self.omniboxCommandHandler;

  NSString* hint =
      base::SysUTF16ToNSString(match.action->GetLabelStrings().hint);
  NSString* suggestionContents = base::SysUTF16ToNSString(
      match.action->GetLabelStrings().suggestion_contents);

  switch (match.action->GetID()) {
    case (int)OmniboxPedalId::PLAY_CHROME_DINO_GAME: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:@"Replace → me"
          accessibilityHint:suggestionContents
                  imageName:@"pedal_dino"
                     action:^{
                       OpenNewTabCommand* command = [OpenNewTabCommand
                           commandWithURLFromChrome:GURL("chrome://dino")
                                        inIncognito:NO];
                       [pedalsEndpoint openURLInNewTab:command];
                     }];
    }
    case (int)OmniboxPedalId::CLEAR_BROWSING_DATA: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:
                       l10n_util::GetNSString(
                           IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_CLEAR_BROWSING_DATA)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_clear_browsing_data"
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showClearBrowsingDataSettings];
                     }];
    }
    case (int)OmniboxPedalId::SET_CHROME_AS_DEFAULT_BROWSER: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_DEFAULT_BROWSER)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_default_browser"
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint
                           showDefaultBrowserSettingsFromViewController:nil];
                     }];
    }
    case (int)OmniboxPedalId::MANAGE_PASSWORDS: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_PASSWORDS)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_passwords"
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint
                           showSavedPasswordsSettingsFromViewController:nil
                                                       showCancelButton:NO];
                     }];
    }
    default:
      return nil;
  }
}

@end
