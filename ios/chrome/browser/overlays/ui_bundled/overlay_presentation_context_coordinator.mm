// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_coordinator.h"

#import <memory>

#import "base/check.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_view_controller.h"

@interface OverlayPresentationContextCoordinator ()
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The presentation context used by OverlayPresenter to drive presentation for
// this container.
@property(nonatomic, assign, readonly)
    OverlayPresentationContextImpl* presentationContext;
@end

@implementation OverlayPresentationContextCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       presentationContext:
                           (OverlayPresentationContextImpl*)context {
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    DCHECK(context);
    _presentationContext = context;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;
  self.started = YES;
  // Create the presentation context view controller and present it over the
  // base view's presentation context.
  _viewController = [[OverlayPresentationContextViewController alloc] init];
  _viewController.definesPresentationContext = YES;
  _viewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  DCHECK(self.baseViewController.definesPresentationContext);
  // Supply the view controller to the presentation context upon completion of
  // the presentation.  If the coordinator is deallocated before the completion
  // block is called, then the presentation context's view controller will
  // remain null, which is correct behavior since the coordinator is unable to
  // support overlay UI presentation.
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    if (!weakSelf)
      return;
    __typeof(self) strongSelf = weakSelf;
    strongSelf.presentationContext->SetPresentationContextViewController(
        strongSelf.viewController);
  };
  [self.baseViewController presentViewController:_viewController
                                        animated:NO
                                      completion:completion];
}

- (void)stop {
  if (!self.started)
    return;
  self.started = NO;
  self.presentationContext->SetPresentationContextViewController(nil);
  // Dismiss the presentation context view controller.
  [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  _viewController = nil;
}

@end
