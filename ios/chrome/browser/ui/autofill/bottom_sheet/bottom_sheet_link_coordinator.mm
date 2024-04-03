// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/bottom_sheet_link_coordinator.h"

#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/bottom_sheet_link_coordinator_delegate.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/bottom_sheet_link_view_controller.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/bottom_sheet_link_view_controller_presentation_delegate.h"
#import "ios/web/public/web_state.h"

@interface BottomSheetLinkCoordinator () <
    BottomSheetLinkViewControllerPresentationDelegate>
@end

@implementation BottomSheetLinkCoordinator {
  UINavigationController* _navigationController;
  BottomSheetLinkViewController* _bottomSheetLinkViewController;
  ChromeBrowserState* _browserState;
  CrURL* _url;
  NSString* _title;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       url:(CrURL*)url
                                     title:(NSString*)title {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _browserState = self.browser->GetBrowserState();
    _url = url;
    _title = title;
  }
  return self;
}

- (void)start {
  _bottomSheetLinkViewController = [[BottomSheetLinkViewController alloc]
      initWithBrowserState:static_cast<web::BrowserState*>(_browserState)
                     title:_title];
  _bottomSheetLinkViewController.presentationDelegate = self;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_bottomSheetLinkViewController];
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:^{
                                        [weakSelf onPresentationCompletion];
                                      }];
}

- (void)stop {
  [_navigationController dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - BottomSheetLinkViewControllerPresentationDelegate

- (void)dismissBottomSheetLinkView {
  [self.delegate dismissBottomSheetLinkCoordinator];
}

#pragma mark - Private

// Called when the navigation controller finishes presenting. We need to
// open the url after the presentation completes.
- (void)onPresentationCompletion {
  [_bottomSheetLinkViewController openURL:_url];
}

@end
