// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/actions/omnibox_action.h"
#import "components/omnibox/browser/actions/omnibox_pedal.h"
#import "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/colorful_background_symbol_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Hard-coded here to avoid dependency on //content. This needs to be kept in
/// sync with kChromeUIScheme in `content/public/common/url_constants.h`.
const char kChromeUIScheme[] = "chrome";

const CGFloat kSymbolSize = 18;
}

@implementation OmniboxPedalAnnotator

- (OmniboxPedalData*)pedalForMatch:(const AutocompleteMatch&)match {
  // Currently this logic takes only pedal type actions, but it could
  // be expanded to support other kinds of actions by changing the
  // predicate or iterating through `match.actions`. In that case,
  // the static_casts below should also be removed in favor of generic
  // use of the OmniboxAction base class.
  const OmniboxPedal* omniboxPedal =
      OmniboxPedal::FromAction(match.GetActionWhere([](const auto& action) {
        return action->ActionId() == OmniboxActionId::PEDAL;
      }));
  if (!omniboxPedal) {
    return nil;
  }
  __weak id<ApplicationCommands> applicationHandler = self.applicationHandler;
  __weak id<SettingsCommands> settingsHandler = self.settingsHandler;
  __weak id<OmniboxCommands> omniboxHandler = self.omniboxHandler;
  __weak id<QuickDeleteCommands> quickDeleteHandler = self.quickDeleteHandler;

  NSString* hint =
      base::SysUTF16ToNSString(omniboxPedal->GetLabelStrings().hint);
  NSString* suggestionContents = base::SysUTF16ToNSString(
      omniboxPedal->GetLabelStrings().suggestion_contents);
  NSInteger pedalType = static_cast<NSInteger>(omniboxPedal->GetMetricsId());
  OmniboxPedalId pedalId = omniboxPedal->PedalId();

  switch (pedalId) {
    case OmniboxPedalId::PLAY_CHROME_DINO_GAME: {
      UIImage* image = CustomSymbolWithPointSize(kDinoSymbol, kSymbolSize);
      NSString* urlStr = [NSString
          stringWithFormat:@"%s://%s", kChromeUIScheme, kChromeUIDinoHost];
      GURL url(base::SysNSStringToUTF8(urlStr));
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:urlStr
          accessibilityHint:suggestionContents
                      image:image
             imageTintColor:UIColor.blackColor
            backgroundColor:UIColor.whiteColor
           imageBorderColor:[UIColor colorNamed:kLightOnlyGrey200Color]
                       type:pedalType
                     action:^{
                       OpenNewTabCommand* command =
                           [OpenNewTabCommand commandWithURLFromChrome:url
                                                           inIncognito:NO];
                       [applicationHandler openURLInNewTab:command];
                     }];
    }
    case OmniboxPedalId::CLEAR_BROWSING_DATA: {
      UIImage* image =
          DefaultSymbolTemplateWithPointSize(kTrashSymbol, kSymbolSize);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:
                       l10n_util::GetNSString(
                           IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_CLEAR_BROWSING_DATA)
          accessibilityHint:suggestionContents
                      image:image
             imageTintColor:nil
            backgroundColor:[UIColor colorNamed:kBlue500Color]
           imageBorderColor:nil
                       type:pedalType
                     action:^{
                       [omniboxHandler cancelOmniboxEdit];
                       if (IsIosQuickDeleteEnabled()) {
                         [quickDeleteHandler
                             showQuickDeleteAndCanPerformTabsClosureAnimation:
                                 YES];
                       } else {
                         [settingsHandler showClearBrowsingDataSettings];
                       }
                     }];
    }
    case OmniboxPedalId::SET_CHROME_AS_DEFAULT_BROWSER: {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
      UIImage* image = MakeSymbolMulticolor(
          CustomSymbolWithPointSize(kMulticolorChromeballSymbol, kSymbolSize));
#else
      UIImage* image = DefaultSymbolTemplateWithPointSize(kDefaultBrowserSymbol,
                                                          kSymbolSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
      DefaultBrowserSettingsPageSource source =
          DefaultBrowserSettingsPageSource::kOmnibox;
      ProceduralBlock action = ^{
        [omniboxHandler cancelOmniboxEdit];
        [settingsHandler showDefaultBrowserSettingsFromViewController:nil
                                                         sourceForUMA:source];
      };
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_DEFAULT_BROWSER)
          accessibilityHint:suggestionContents
                      image:image
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
             imageTintColor:nil
            backgroundColor:UIColor.whiteColor
           imageBorderColor:[UIColor colorNamed:kLightOnlyGrey200Color]
#else
             imageTintColor:nil
            backgroundColor:[UIColor colorNamed:kPurple500Color]
           imageBorderColor:nil
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
                       type:pedalType
                     action:action];
    }
    case OmniboxPedalId::MANAGE_PASSWORDS: {
      UIImage* image = CustomSymbolWithPointSize(kPasswordSymbol, kSymbolSize);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_PASSWORDS)
          accessibilityHint:suggestionContents
                      image:image
             imageTintColor:nil
            backgroundColor:[UIColor colorNamed:kYellow500Color]
           imageBorderColor:nil
                       type:pedalType
                     action:^{
                       [omniboxHandler cancelOmniboxEdit];
                       [settingsHandler
                           showSavedPasswordsSettingsFromViewController:nil
                                                       showCancelButton:NO];
                     }];
    }
    case OmniboxPedalId::UPDATE_CREDIT_CARD: {
      UIImage* image =
          DefaultSymbolTemplateWithPointSize(kCreditCardSymbol, kSymbolSize);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:
                       l10n_util::GetNSString(
                           IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_UPDATE_CREDIT_CARD)
          accessibilityHint:suggestionContents
                      image:image
             imageTintColor:nil
            backgroundColor:[UIColor colorNamed:kYellow500Color]
           imageBorderColor:nil
                       type:pedalType
                     action:^{
                       [omniboxHandler cancelOmniboxEdit];
                       [settingsHandler showCreditCardSettings];
                     }];
    }
    case OmniboxPedalId::LAUNCH_INCOGNITO: {
      UIImage* image = CustomSymbolWithPointSize(kIncognitoSymbol, kSymbolSize);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_LAUNCH_INCOGNITO)
          accessibilityHint:suggestionContents
                      image:image
             imageTintColor:nil
            backgroundColor:[UIColor colorNamed:kGrey800Color]
           imageBorderColor:nil
                       type:pedalType
                     action:^{
                       [omniboxHandler cancelOmniboxEdit];
                       [applicationHandler
                           openURLInNewTab:[OpenNewTabCommand
                                               incognitoTabCommand]];
                     }];
    }
    case OmniboxPedalId::RUN_CHROME_SAFETY_CHECK: {
      UIImage* image =
          CustomSymbolWithPointSize(kSafetyCheckSymbol, kSymbolSize);
      NSString* subtitle = l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_RUN_CHROME_SAFETY_CHECK);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:subtitle
          accessibilityHint:suggestionContents
                      image:image
             imageTintColor:nil
            backgroundColor:[UIColor colorNamed:kBlue500Color]
           imageBorderColor:nil
                       type:pedalType
                     action:^{
                       [omniboxHandler cancelOmniboxEdit];
                       [settingsHandler
                           showAndStartSafetyCheckForReferrer:
                               password_manager::PasswordCheckReferrer::
                                   kSafetyCheck];
                     }];
    }
    case OmniboxPedalId::MANAGE_CHROME_SETTINGS: {
      UIImage* image =
          DefaultSymbolTemplateWithPointSize(kSettingsSymbol, kSymbolSize);
      NSString* subtitle = l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_CHROME_SETTINGS);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:subtitle
          accessibilityHint:suggestionContents
                      image:image
             imageTintColor:nil
            backgroundColor:[UIColor colorNamed:kGrey500Color]
           imageBorderColor:nil
                       type:pedalType
                     action:^{
                       [omniboxHandler cancelOmniboxEdit];
                       [applicationHandler showSettingsFromViewController:nil];
                     }];
    }
    case OmniboxPedalId::VIEW_CHROME_HISTORY: {
      UIImage* image =
          DefaultSymbolTemplateWithPointSize(kHistorySymbol, kSymbolSize);
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:
                       l10n_util::GetNSString(
                           IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_VIEW_CHROME_HISTORY)
          accessibilityHint:suggestionContents
                      image:image
             imageTintColor:nil
            backgroundColor:[UIColor colorNamed:kBlue500Color]
           imageBorderColor:nil
                       type:pedalType
                     action:^{
                       [omniboxHandler cancelOmniboxEdit];
                       [applicationHandler showHistory];
                     }];
    }
      // If a new case is added here, make sure to update the method returning
      // the icon.
    default:
      return nil;
  }
}

@end
