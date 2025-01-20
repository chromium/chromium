// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_overflow_menu_factory.h"

#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
namespace {

const CGFloat kMenuSymbolSize = 18;

}  // namespace
#endif

@implementation LensOverlayOverflowMenuFactory {
  BrowserActionFactory* _actionFactory;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _actionFactory = [[BrowserActionFactory alloc]
        initWithBrowser:browser
               scenario:kMenuScenarioHistogramHistoryEntry];
  }

  return self;
}

- (UIAction*)openUserActivityAction {
  UIAction* action = [self openURLAction:GURL(kMyActivityURL)];
  action.title = l10n_util::GetNSString(IDS_IOS_MY_ACTIVITY_TITLE);
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  action.image = MakeSymbolMonochrome(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kMenuSymbolSize));
#else
  action.image = nil;
#endif
  return action;
}

- (UIAction*)learnMoreAction {
  UIAction* action = [self openURLAction:GURL(kLearnMoreLensURL)];
  action.title = l10n_util::GetNSString(IDS_IOS_LENS_LEARN_MORE);
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  action.image = MakeSymbolMonochrome(
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kMenuSymbolSize));
#else
  action.image = nil;
#endif
  return action;
}

- (UIAction*)openURLAction:(GURL)URL {
  return [_actionFactory actionToOpenInNewTabWithURL:URL completion:nil];
}

@end
