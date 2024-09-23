// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/share_download_overlay_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/share_download_overlay_commands.h"
#import "ios/chrome/browser/ui/sharing/share_download_overlay_view_controller.h"
#import "ios/web/public/web_state.h"

namespace {

// Duration to show or hide the `overlayedView_`.
const NSTimeInterval kOverlayViewAnimationDuration = 0.3;

}  // namespace

@interface ShareDownloadOverlayCoordinator () {
  UIView* _webView;
}

// Download overlay view controller.
@property(nonatomic, strong) ShareDownloadOverlayViewController* viewController;

@end

@implementation ShareDownloadOverlayCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   webView:(UIView*)webView {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    DCHECK(webView);
    _webView = webView;
  }
  return self;
}
- (void)start {
  id<ShareDownloadOverlayCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ShareDownloadOverlayCommands);

  self.viewController = [[ShareDownloadOverlayViewController alloc]
      initWithBaseView:std::exchange(_webView, nil)
               handler:handler];

  UIView* overlayedView = self.viewController.view;
  [UIView animateWithDuration:kOverlayViewAnimationDuration
                   animations:^{
                     [overlayedView setAlpha:1.0];
                   }];
}

- (void)stop {
  if (!self.viewController.view)
    return;

  UIView* overlayedView = self.viewController.view;
  [UIView animateWithDuration:kOverlayViewAnimationDuration
      animations:^{
        [overlayedView setAlpha:0.0];
      }
      completion:^(BOOL finished) {
        [overlayedView removeFromSuperview];
      }];

  self.viewController = nil;
}

@end
