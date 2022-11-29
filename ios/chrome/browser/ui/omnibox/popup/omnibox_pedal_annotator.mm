// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/actions/omnibox_action.h"
#import "components/omnibox/browser/actions/omnibox_pedal.h"
#import "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/default_browser/promo_source.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

/// Hard-coded here to avoid dependency on //content. This needs to be kept in
/// sync with kChromeUIScheme in `content/public/common/url_constants.h`.
const char kChromeUIScheme[] = "chrome";

}

@implementation OmniboxPedalAnnotator

- (OmniboxPedalData*)pedalForMatch:(const AutocompleteMatch&)match
                         incognito:(BOOL)incognito {
  if (!match.action ||
      match.action->GetID() >= static_cast<int>(OmniboxPedalId::TOTAL_COUNT)) {
    return nil;
  }
  __weak id<ApplicationCommands> pedalsEndpoint = self.pedalsEndpoint;
  __weak id<OmniboxCommands> omniboxCommandHandler = self.omniboxCommandHandler;

  NSString* hint =
      base::SysUTF16ToNSString(match.action->GetLabelStrings().hint);
  NSString* suggestionContents = base::SysUTF16ToNSString(
      match.action->GetLabelStrings().suggestion_contents);
  NSInteger pedalType = static_cast<NSInteger>(
      static_cast<OmniboxPedal*>(match.action.get())->GetMetricsId());

  switch (static_cast<OmniboxPedalId>(match.action->GetID())) {
    case OmniboxPedalId::PLAY_CHROME_DINO_GAME: {
      NSString* urlStr = [NSString
          stringWithFormat:@"%s://%s", kChromeUIScheme, kChromeUIDinoHost];
      GURL url(base::SysNSStringToUTF8(urlStr));
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:urlStr
          accessibilityHint:suggestionContents
                  imageName:@"pedal_dino"
                       type:pedalType
                  incognito:incognito
                     action:^{
                       OpenNewTabCommand* command =
                           [OpenNewTabCommand commandWithURLFromChrome:url
                                                           inIncognito:NO];
                       [pedalsEndpoint openURLInNewTab:command];
                     }];
    }
    case OmniboxPedalId::CLEAR_BROWSING_DATA: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:
                       l10n_util::GetNSString(
                           IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_CLEAR_BROWSING_DATA)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_clear_browsing_data"
                       type:pedalType
                  incognito:incognito
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showClearBrowsingDataSettings];
                     }];
    }
    case OmniboxPedalId::SET_CHROME_AS_DEFAULT_BROWSER: {
      ProceduralBlock action = ^{
        [omniboxCommandHandler cancelOmniboxEdit];
        [pedalsEndpoint
            showDefaultBrowserSettingsFromViewController:nil
                                            sourceForUMA:
                                                DefaultBrowserPromoSource::
                                                    kOmnibox];
      };
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_DEFAULT_BROWSER)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_default_browser"
                       type:pedalType
                  incognito:incognito
                     action:action];
    }
    case OmniboxPedalId::MANAGE_PASSWORDS: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_PASSWORDS)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_passwords"
                       type:pedalType
                  incognito:incognito
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint
                           showSavedPasswordsSettingsFromViewController:nil
                                                       showCancelButton:NO];
                     }];
    }
    case OmniboxPedalId::UPDATE_CREDIT_CARD: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:
                       l10n_util::GetNSString(
                           IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_UPDATE_CREDIT_CARD)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_payments"
                       type:pedalType
                  incognito:incognito
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showCreditCardSettings];
                     }];
    }
    case OmniboxPedalId::LAUNCH_INCOGNITO: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_LAUNCH_INCOGNITO)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_incognito"
                       type:pedalType
                  incognito:incognito
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint
                           openURLInNewTab:[OpenNewTabCommand
                                               incognitoTabCommand]];
                     }];
    }
    case OmniboxPedalId::RUN_CHROME_SAFETY_CHECK: {
      NSString* subtitle = l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_RUN_CHROME_SAFETY_CHECK);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:subtitle
          accessibilityHint:suggestionContents
                  imageName:@"pedal_safety_check"
                       type:pedalType
                  incognito:incognito
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint
                           showSafetyCheckSettingsAndStartSafetyCheck];
                     }];
    }
    case OmniboxPedalId::MANAGE_CHROME_SETTINGS: {
      NSString* subtitle = l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_CHROME_SETTINGS);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:subtitle
          accessibilityHint:suggestionContents
                  imageName:@"pedal_settings"
                       type:pedalType
                  incognito:incognito
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showSettingsFromViewController:nil];
                     }];
    }
    case OmniboxPedalId::VIEW_CHROME_HISTORY: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:
                       l10n_util::GetNSString(
                           IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_VIEW_CHROME_HISTORY)
          accessibilityHint:suggestionContents
                  imageName:@"pedal_history"
                       type:pedalType
                  incognito:incognito
                     action:^{
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showHistory];
                     }];
    }
    default:
      return nil;
  }
}

@end
