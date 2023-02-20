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

  UIImage* image = [self pedalIconForPedalId:pedalId incognito:incognito];

  switch (pedalId) {
    case OmniboxPedalId::PLAY_CHROME_DINO_GAME: {
      NSString* urlStr = [NSString
          stringWithFormat:@"%s://%s", kChromeUIScheme, kChromeUIDinoHost];
      GURL url(base::SysNSStringToUTF8(urlStr));
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:urlStr
          accessibilityHint:suggestionContents
                      image:image
                       type:pedalType
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
                      image:image
                       type:pedalType
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
                      image:image
                       type:pedalType
                     action:action];
    }
    case OmniboxPedalId::MANAGE_PASSWORDS: {
      return [[OmniboxPedalData alloc]
              initWithTitle:hint
                   subtitle:l10n_util::GetNSString(
                                IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_PASSWORDS)
          accessibilityHint:suggestionContents
                      image:image
                       type:pedalType
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
                      image:image
                       type:pedalType
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
                      image:image
                       type:pedalType
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
                      image:image
                       type:pedalType
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
                      image:image
                       type:pedalType
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
                      image:image
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

#pragma mark - Private

// Returns the image associated with `pedalId`, for `incognito` or not.
- (UIImage*)pedalIconForPedalId:(OmniboxPedalId)pedalId
                      incognito:(BOOL)incognito {
  ColorfulBackgroundSymbolView* symbolView =
      [[ColorfulBackgroundSymbolView alloc] init];
  if (incognito) {
    // Dark mode is set explicitly if incognito is enabled.
    symbolView.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }

  // Dark mode is set explicitly if incognito is enabled.
  UITraitCollection* traitCollection =
      [UITraitCollection traitCollectionWithUserInterfaceStyle:
                             incognito ? UIUserInterfaceStyleDark
                                       : UIUserInterfaceStyleUnspecified];

  switch (pedalId) {
    case OmniboxPedalId::PLAY_CHROME_DINO_GAME: {
      if (UseSymbolsInOmnibox()) {
        [symbolView setSymbol:CustomSymbolWithPointSize(kDinoSymbol, 22)];
        symbolView.backgroundColor = UIColor.whiteColor;
        [symbolView setSymbolTintColor:UIColor.blackColor];
        symbolView.borderColor = [UIColor colorNamed:kGrey200Color];
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_dino"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    case OmniboxPedalId::CLEAR_BROWSING_DATA: {
      if (UseSymbolsInOmnibox()) {
        [symbolView setSymbolName:kTrashSymbol systemSymbol:YES];
        symbolView.backgroundColor = [UIColor colorNamed:kBlue500Color];
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_clear_browsing_data"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    case OmniboxPedalId::SET_CHROME_AS_DEFAULT_BROWSER: {
      if (UseSymbolsInOmnibox()) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
        symbolView.backgroundColor = UIColor.whiteColor;
        [symbolView setSymbol:MakeSymbolMulticolor(CustomSymbolWithPointSize(
                                  kChromeSymbol, 22))];
        symbolView.borderColor = [UIColor colorNamed:kGrey200Color];
#else
        [symbolView setSymbolName:kDefaultBrowserSymbol systemSymbol:YES];
        symbolView.backgroundColor = [UIColor colorNamed:kPurple500Color];
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_default_browser"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    case OmniboxPedalId::MANAGE_PASSWORDS: {
      if (UseSymbolsInOmnibox()) {
        [symbolView setSymbolName:kPasswordSymbol systemSymbol:NO];
        symbolView.backgroundColor = [UIColor colorNamed:kYellow500Color];
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_passwords"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    case OmniboxPedalId::UPDATE_CREDIT_CARD: {
      if (UseSymbolsInOmnibox()) {
        [symbolView setSymbolName:kCreditCardSymbol systemSymbol:YES];
        symbolView.backgroundColor = [UIColor colorNamed:kYellow500Color];
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_payments"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    case OmniboxPedalId::LAUNCH_INCOGNITO: {
      if (UseSymbolsInOmnibox()) {
        [symbolView setSymbolName:kIncognitoSymbol systemSymbol:NO];
        symbolView.backgroundColor = [UIColor colorNamed:kGrey800Color];
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_incognito"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    case OmniboxPedalId::RUN_CHROME_SAFETY_CHECK: {
      if (UseSymbolsInOmnibox()) {
        [symbolView setSymbolName:kSafetyCheckSymbol systemSymbol:NO];
        symbolView.backgroundColor = [UIColor colorNamed:kBlue500Color];
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_safety_check"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    case OmniboxPedalId::MANAGE_CHROME_SETTINGS: {
      if (UseSymbolsInOmnibox()) {
        [symbolView setSymbolName:kSettingsSymbol systemSymbol:YES];
        symbolView.backgroundColor = [UIColor colorNamed:kGrey500Color];
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_settings"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    case OmniboxPedalId::VIEW_CHROME_HISTORY: {
      if (UseSymbolsInOmnibox()) {
        [symbolView setSymbolName:kHistorySymbol systemSymbol:YES];
        symbolView.backgroundColor = [UIColor colorNamed:kBlue500Color];
        return ImageFromView(symbolView, nil, UIEdgeInsetsZero);
      } else {
        return [UIImage imageNamed:@"pedal_history"
                                 inBundle:nil
            compatibleWithTraitCollection:traitCollection];
      }
    }
    default:
      NOTREACHED();
      return nil;
  }
}

@end
