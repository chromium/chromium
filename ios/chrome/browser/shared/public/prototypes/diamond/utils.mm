// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/prototypes/diamond/utils.h"

#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/prototypes/diamond/new_tab_prototype_view_controller.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Returns whether the call is currently originating from incognito.
bool IsInIncognito(bool from_tab_grid,
                   bool incognito_grid,
                   Browser* regular_browser,
                   Browser* incognito_browser) {
  if (from_tab_grid) {
    return incognito_grid;
  }
  id<BrowserProviderInterface> browser_provider =
      regular_browser->GetSceneState().browserProviderInterface;
  return (browser_provider.currentBrowserProvider ==
          browser_provider.incognitoBrowserProvider);
}

}  // namespace

const CGFloat kChromeAppBarPrototypeHeight = 49;
const CGFloat kChromeAppBarPrototypeSymbolSize = 23;

const CGFloat kDiamondBrowserCornerRadius = 22;

const CGFloat kDiamondToolbarHeight = 62;
const CGFloat kDiamondCollapsedToolbarHeight = 41;
const CGFloat kDiamondLocationBarHeight = 46;

NSString* kDiamondEnterTabGridNotification =
    @"kDiamondEnterTabGridNotification";
NSString* kDiamondLeaveTabGridNotification =
    @"kDiamondLeaveTabGridNotification";
NSString* kDiamondLongPressButton = @"kDiamondLongPressButton";

NSString* const kAppSymbol = @"app";
NSString* const kAppFillSymbol = @"app.fill";

void DiamondPrototypeStartGemini(bool from_tab_grid,
                                 bool incognito_grid,
                                 Browser* regular_browser,
                                 Browser* incognito_browser,
                                 UIViewController* base_view_controller) {
  CHECK(IsDiamondPrototypeEnabled());
  bool incognito = IsInIncognito(from_tab_grid, incognito_grid, regular_browser,
                                 incognito_browser);
  Browser* browser = incognito ? incognito_browser : regular_browser;
  if (from_tab_grid || incognito) {
    UIViewController* coming_soon_view_controller =
        [[UIViewController alloc] init];
    UILabel* label = [[UILabel alloc] init];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.text = @"Coming soon...";
    label.textColor = [UIColor colorNamed:kTextPrimaryColor];
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    [coming_soon_view_controller.view addSubview:label];
    [NSLayoutConstraint activateConstraints:@[
      [coming_soon_view_controller.view.centerXAnchor
          constraintEqualToAnchor:label.centerXAnchor],
      [coming_soon_view_controller.view.centerYAnchor
          constraintEqualToAnchor:label.centerYAnchor],
    ]];
    coming_soon_view_controller.view.backgroundColor =
        [UIColor colorNamed:kPrimaryBackgroundColor];
    coming_soon_view_controller.sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent mediumDetent] ];
    [base_view_controller presentViewController:coming_soon_view_controller
                                       animated:YES
                                     completion:nil];
  } else {
    id<BWGCommands> BWGCommandsHandler =
        HandlerForProtocol(browser->GetCommandDispatcher(), BWGCommands);
    [BWGCommandsHandler startBWGFlowWithEntryPoint:bwg::EntryPoint::Diamond];
  }
}

void DiamondPrototypeStartNewTab(bool from_tab_grid,
                                 bool incognito_grid,
                                 Browser* regular_browser,
                                 Browser* incognito_browser,
                                 UIViewController* base_view_controller) {
  CHECK(IsDiamondPrototypeEnabled());
  bool incognito = IsInIncognito(from_tab_grid, incognito_grid, regular_browser,
                                 incognito_browser);
  Browser* browser = incognito ? incognito_browser : regular_browser;

  NewTabPrototypeViewController* new_tab_view_controller =
      [[NewTabPrototypeViewController alloc]
          initWithBaseViewController:base_view_controller
                             browser:browser
                        isNewTabPage:YES
                   shouldExitTabGrid:from_tab_grid];
  [base_view_controller presentViewController:new_tab_view_controller
                                     animated:YES
                                   completion:nil];
}

namespace {

// Returns the symbol configuration for the app bar.
UIImageSymbolConfiguration* GetAppBarSymbolConfiguration() {
  return [UIImageSymbolConfiguration
      configurationWithPointSize:kChromeAppBarPrototypeSymbolSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
}

}  // namespace

UIImage* GetDefaultAppBarSymbol(NSString* symbol_name) {
  return DefaultSymbolWithConfiguration(symbol_name,
                                        GetAppBarSymbolConfiguration());
}

UIImage* GetCustomAppBarSymbol(NSString* symbol_name) {
  return CustomSymbolWithConfiguration(symbol_name,
                                       GetAppBarSymbolConfiguration());
}
