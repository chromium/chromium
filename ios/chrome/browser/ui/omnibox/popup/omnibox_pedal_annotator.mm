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
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/icons/colorful_background_symbol_view.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

/// Hard-coded here to avoid dependency on //content. This needs to be kept in
/// sync with kChromeUIScheme in `content/public/common/url_constants.h`.
const char kChromeUIScheme[] = "chrome";

const CGFloat kSymbolSize = 18;
}

@implementation OmniboxPedalAnnotator

- (OmniboxPedalData*)pedalForMatch:(const AutocompleteMatch&)match
                         incognito:(BOOL)incognito {
  // Currently this logic takes only pedal type actions, but it could
  // be expanded to support other kinds of actions by changing the
  // predicate or iterating through `match.actions`. In that case,
  // the static_casts below should also be removed in favor of generic
  // use of the OmniboxAction base class.
  const OmniboxAction* pedalAction =
      match.GetActionWhere([](const auto& action) {
        // Pedal action is a match action with an ID in
        // 0 ..< OmniboxPedalId::TOTAL_COUNT range (and ID 0 is NONE).
        return action->GetID() < static_cast<int>(OmniboxPedalId::TOTAL_COUNT);
      });
  if (!pedalAction) {
    return nil;
  }
  __weak id<ApplicationCommands> pedalsEndpoint = self.pedalsEndpoint;
  __weak id<OmniboxCommands> omniboxCommandHandler = self.omniboxCommandHandler;

  NSString* hint =
      base::SysUTF16ToNSString(pedalAction->GetLabelStrings().hint);
  NSString* suggestionContents = base::SysUTF16ToNSString(
      pedalAction->GetLabelStrings().suggestion_contents);
  NSInteger pedalType = static_cast<NSInteger>(
      static_cast<const OmniboxPedal*>(pedalAction)->GetMetricsId());

  OmniboxPedalId pedalId = static_cast<OmniboxPedalId>(pedalAction->GetID());

  UIImage* image;

  // Dark mode is set explicitly if incognito is enabled.
  UITraitCollection* traitCollection =
      [UITraitCollection traitCollectionWithUserInterfaceStyle:
                             incognito ? UIUserInterfaceStyleDark
                                       : UIUserInterfaceStyleUnspecified];

  switch (pedalId) {
    case OmniboxPedalId::PLAY_CHROME_DINO_GAME: {
      if (UseSymbolsInOmnibox()) {
        image = CustomSymbolWithPointSize(kDinoSymbol, kSymbolSize);
      } else {
        image = [UIImage imageNamed:@"pedal_dino"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
                       [pedalsEndpoint openURLInNewTab:command];
                     }];
    }
    case OmniboxPedalId::CLEAR_BROWSING_DATA: {
      if (UseSymbolsInOmnibox()) {
        image = DefaultSymbolTemplateWithPointSize(kTrashSymbol, kSymbolSize);
      } else {
        image = [UIImage imageNamed:@"pedal_clear_browsing_data"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showClearBrowsingDataSettings];
                     }];
    }
    case OmniboxPedalId::SET_CHROME_AS_DEFAULT_BROWSER: {
      if (UseSymbolsInOmnibox()) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
        image = MakeSymbolMulticolor(
            CustomSymbolWithPointSize(kChromeSymbol, kSymbolSize));
#else
        image = DefaultSymbolTemplateWithPointSize(kDefaultBrowserSymbol,
                                                   kSymbolSize);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
      } else {
        image = [UIImage imageNamed:@"pedal_default_browser"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
      if (UseSymbolsInOmnibox()) {
        image = CustomSymbolWithPointSize(kPasswordSymbol, kSymbolSize);
      } else {
        image = [UIImage imageNamed:@"pedal_passwords"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint
                           showSavedPasswordsSettingsFromViewController:nil
                                                       showCancelButton:NO
                                                     startPasswordCheck:NO];
                     }];
    }
    case OmniboxPedalId::UPDATE_CREDIT_CARD: {
      if (UseSymbolsInOmnibox()) {
        image =
            DefaultSymbolTemplateWithPointSize(kCreditCardSymbol, kSymbolSize);
      } else {
        image = [UIImage imageNamed:@"pedal_payments"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showCreditCardSettings];
                     }];
    }
    case OmniboxPedalId::LAUNCH_INCOGNITO: {
      if (UseSymbolsInOmnibox()) {
        image = CustomSymbolWithPointSize(kIncognitoSymbol, kSymbolSize);
      } else {
        image = [UIImage imageNamed:@"pedal_incognito"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint
                           openURLInNewTab:[OpenNewTabCommand
                                               incognitoTabCommand]];
                     }];
    }
    case OmniboxPedalId::RUN_CHROME_SAFETY_CHECK: {
      if (UseSymbolsInOmnibox()) {
        image = CustomSymbolWithPointSize(kSafetyCheckSymbol, kSymbolSize);
      } else {
        image = [UIImage imageNamed:@"pedal_safety_check"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint
                           showSafetyCheckSettingsAndStartSafetyCheck];
                     }];
    }
    case OmniboxPedalId::MANAGE_CHROME_SETTINGS: {
      if (UseSymbolsInOmnibox()) {
        image =
            DefaultSymbolTemplateWithPointSize(kSettingsSymbol, kSymbolSize);
      } else {
        image = [UIImage imageNamed:@"pedal_settings"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showSettingsFromViewController:nil];
                     }];
    }
    case OmniboxPedalId::VIEW_CHROME_HISTORY: {
      if (UseSymbolsInOmnibox()) {
        image = DefaultSymbolTemplateWithPointSize(kHistorySymbol, kSymbolSize);
      } else {
        image = [UIImage imageNamed:@"pedal_history"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
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
                       [omniboxCommandHandler cancelOmniboxEdit];
                       [pedalsEndpoint showHistory];
                     }];
    }
      // If a new case is added here, make sure to update the method returning
      // the icon.
    default:
      return nil;
  }
}

@end
