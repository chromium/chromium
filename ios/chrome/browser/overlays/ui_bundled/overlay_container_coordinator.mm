// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator+initialization.h"

#import <memory>

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_view_controller.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_coordinator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_impl_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface OverlayContainerCoordinator () <
    OverlayContainerViewControllerDelegate,
    OverlayPresentationContextImplDelegate>
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The presentation context used by OverlayPresenter to drive presentation for
// this container.
@property(nonatomic, assign, readonly)
    OverlayPresentationContextImpl* presentationContext;
// The coordinator that manages the base view for overlay UI displayed using
// UIViewController presentation.
@property(nonatomic, strong)
    OverlayPresentationContextCoordinator* presentationContextCoordinator;
@end

@implementation OverlayContainerCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  modality:(OverlayModality)modality {
  OverlayPresentationContextImpl* context =
      OverlayPresentationContextImpl::FromBrowser(browser, modality);
  return [self initWithBaseViewController:viewController
                                  browser:browser
                      presentationContext:context];
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;
  self.started = YES;
  // Create the container view controller.
  OverlayContainerViewController* viewController =
      [[OverlayContainerViewController alloc] init];
  viewController.definesPresentationContext = YES;
  viewController.delegate = self;
  _viewController = viewController;
  // Set the coordinator as the delegate for the presentation context.
  self.presentationContext->SetDelegate(self);
  // Create the presentation context coordinator.  It is started when the
  // container view controller is finished being added to the window, as its
  // presentation would no-op if started too early.
  self.presentationContextCoordinator =
      [[OverlayPresentationContextCoordinator alloc]
          initWithBaseViewController:_viewController
                             browser:self.browser
                 presentationContext:self.presentationContext];
  // Add the container view controller to the hierarchy.
  UIView* containerView = _viewController.view;
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:containerView];
  AddSameConstraints(containerView, self.baseViewController.view);
  [_viewController didMoveToParentViewController:self.baseViewController];
}

- (void)stop {
  if (!self.started)
    return;
  self.presentationContext->SetDelegate(nil);
  // Clean up the presentation context coordinator.
  [self.presentationContextCoordinator stop];
  self.presentationContextCoordinator = nil;
  // Remove the container view and reset the view controller.
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
  self.started = NO;
}

#pragma mark - OverlayContainerViewControllerDelegate

- (void)containerViewController:
            (OverlayContainerViewController*)containerViewController
                didMoveToWindow:(UIWindow*)window {
  self.presentationContext->SetWindow(window);
}

#pragma mark - OverlayPresentationContextImplDelegate

- (void)updatePresentationContext:(OverlayPresentationContextImpl*)context
      forPresentationCapabilities:
          (OverlayPresentationContext::UIPresentationCapabilities)capabilities {
  DCHECK_EQ(self.presentationContext, context);
  DCHECK(self.started);
  DCHECK(self.viewController);

  // Update the context's container view controller.
  bool needsContainer =
      capabilities &
      OverlayPresentationContext::UIPresentationCapabilities::kContained;
  self.presentationContext->SetContainerViewController(
      needsContainer ? self.viewController : nil);

  // Start or stop the presentation context coordinator depending on whether
  // it is required to support `capabilities`.
  if (capabilities &
      OverlayPresentationContext::UIPresentationCapabilities::kPresented) {
    // The coordinator cannot be started if its base UIViewController doesn't
    // belong to a window.  The context will re-request the kPresented
    // capability when the view moves to a window.
    if (self.viewController.view.window)
      [self.presentationContextCoordinator start];
  } else {
    [self.presentationContextCoordinator stop];
  }
}

@end

@implementation OverlayContainerCoordinator (Initialization)

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

@end
