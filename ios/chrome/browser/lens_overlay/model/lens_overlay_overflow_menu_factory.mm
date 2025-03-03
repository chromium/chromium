// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_overflow_menu_factory.h"

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_overflow_menu_delegate.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
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
  raw_ptr<Browser> _browser;
  BrowserActionFactory* _actionFactory;
  __weak id<LensOverlayOverflowMenuDelegate> _overflowMenuDelegate;
}

- (instancetype)initWithBrowser:(Browser*)browser
           overflowMenuDelegate:
               (id<LensOverlayOverflowMenuDelegate>)overflowMenuDelegate {
  self = [super init];
  if (self) {
    _browser = browser;
    _actionFactory = [[BrowserActionFactory alloc]
        initWithBrowser:browser
               scenario:kMenuScenarioHistogramHistoryEntry];
    _overflowMenuDelegate = overflowMenuDelegate;
  }

  return self;
}

- (UIAction*)openUserActivityAction {
  NSString* title = l10n_util::GetNSString(IDS_IOS_MY_ACTIVITY_TITLE);
  UIImage* image;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  image = MakeSymbolMonochrome(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kMenuSymbolSize));
#endif

  if (IsLensOverlaySameTabNavigationEnabled(
          _browser->GetProfile()->GetPrefs())) {
    return [self openURLInTheSameTabAction:GURL(kMyActivityURL)
                                     title:title
                                     image:image];
  }

  return [self openURLInNewTabAction:GURL(kMyActivityURL)
                               title:title
                               image:image];
}

- (UIAction*)learnMoreAction {
  NSString* title = l10n_util::GetNSString(IDS_IOS_LENS_LEARN_MORE);
  UIImage* image;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  image = MakeSymbolMonochrome(
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kMenuSymbolSize));
#endif

  if (IsLensOverlaySameTabNavigationEnabled(
          _browser->GetProfile()->GetPrefs())) {
    return [self openURLInTheSameTabAction:GURL(kLearnMoreLensURL)
                                     title:title
                                     image:image];
  }

  return [self openURLInNewTabAction:GURL(kLearnMoreLensURL)
                               title:title
                               image:image];
}

- (UIAction*)openURLInNewTabAction:(GURL)URL
                             title:(NSString*)title
                             image:(UIImage*)image {
  UIAction* action = [_actionFactory actionToOpenInNewTabWithURL:URL
                                                      completion:nil];
  action.title = title;
  action.image = image;
  return action;
}

- (UIAction*)openURLInTheSameTabAction:(GURL)URL
                                 title:(NSString*)title
                                 image:(UIImage*)image {
  __weak id<LensOverlayOverflowMenuDelegate> weakOverflowMenuDelegate =
      _overflowMenuDelegate;
  return [UIAction actionWithTitle:title
                             image:image
                        identifier:nil
                           handler:^(UIAction* action) {
                             [weakOverflowMenuDelegate openActionURL:URL];
                           }];
}

- (UIAction*)searchWithCameraActionWithHandler:(void (^)())handler {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_SPEEDBUMP_MENU_CAMERA);
  UIImage* image;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  image = MakeSymbolMonochrome(
      CustomSymbolWithPointSize(kCameraLensSymbol, kMenuSymbolSize));
#endif

  return [UIAction actionWithTitle:title
                             image:image
                        identifier:nil
                           handler:^(UIAction* action) {
                             if (handler) {
                               handler();
                             }
                           }];
}

@end
