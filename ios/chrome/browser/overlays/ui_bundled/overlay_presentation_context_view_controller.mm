// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_view_controller.h"

#import <cmath>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_controller.h"

namespace {
// Returns YES if `size1` and `size2` are less than 0.5 different in both width
// and height.
BOOL CGSizeAlmostEqualToSize(CGSize size1, CGSize size2) {
  const CGFloat kMinimumSizeChange = 0.5;
  return std::fabs(size1.height - size2.height) <= kMinimumSizeChange &&
         std::fabs(size1.width - size2.width) <= kMinimumSizeChange;
}
}  // namespace

@interface OverlayPresentationContextViewController ()
// The view used to lay out the presentation context.  The presentation context
// view is resized to match `layoutView`'s frame.
@property(nonatomic, weak) UIView* layoutView;
// Whether the presented view controller is presented using an
// OverlayPresentationController whose `resizesPresentationContainer` property
// returns YES.
@property(nonatomic, readonly) BOOL presentedViewControllerResizesContainer;
@end

@implementation OverlayPresentationContextViewController

#pragma mark - Accessors

- (BOOL)presentedViewControllerResizesContainer {
  // The non-strict cast returns nil if the presented UIViewController does not
  // use an OverlayPresentationController.  This results in this selector
  // returning NO for these UIViewControllers.
  return base::apple::ObjCCast<OverlayPresentationController>(
             self.presentedViewController.presentationController)
      .resizesPresentationContainer;
}

#pragma mark - UIViewController

- (void)viewDidLayoutSubviews {
  UIView* view = self.view;
  UIView* containerView = self.presentationController.containerView;
  UIView* newLayoutView = [self currentLayoutView];
  UIWindow* window = newLayoutView.window;

  CGRect oldLayoutFrame = self.layoutView.frame;
  CGRect newLayoutFrame = [window convertRect:newLayoutView.bounds
                                     fromView:newLayoutView];
  if (CGSizeAlmostEqualToSize(containerView.bounds.size, oldLayoutFrame.size) &&
      CGSizeAlmostEqualToSize(newLayoutFrame.size, oldLayoutFrame.size)) {
    return;
  }
  self.layoutView = newLayoutView;

  // Lay out the presentation context and its container view to match
  // `layoutView`, if size has changed.
  if (self.layoutView) {
    containerView.frame = [containerView.superview convertRect:newLayoutFrame
                                                      fromView:window];
    view.frame = [view.superview convertRect:newLayoutFrame fromView:window];
  } else {
    containerView.frame = CGRectZero;
    view.frame = CGRectZero;
  }
  // If `layoutView` is not laid out using constraints, its frame may have been
  // updated by the container and presentation context layout above.  Reset the
  // frame by converting back from the window coordinates.
  if (self.presentedViewControllerResizesContainer &&
      self.layoutView.translatesAutoresizingMaskIntoConstraints) {
    self.layoutView.frame =
        [self.layoutView.superview convertRect:newLayoutFrame fromView:window];
  }
}

- (void)presentViewController:(UIViewController*)viewController
                     animated:(BOOL)animated
                   completion:(void (^)(void))completion {
  // Trigger a layout of the presentation context before presenting.  This
  // allows the presentation context to be resized appropriately during the the
  // presentation animation.
  [self.view setNeedsLayout];

  [super presentViewController:viewController
                      animated:animated
                    completion:completion];
}

- (void)dismissViewControllerAnimated:(BOOL)animated
                           completion:(void (^)(void))completion {
  // Create an updated completion block that triggers layout after the dismissal
  // finishes.  This will resize the presentation context to CGRectZero so that
  // touches can be handled by the underlying browser UI once the presented
  // overlay is removed.
  [super dismissViewControllerAnimated:animated
                            completion:^{
                              if (completion)
                                completion();
                              [self.view setNeedsLayout];
                            }];
}

#pragma mark - Private

// Returns the current layout view.
- (UIView*)currentLayoutView {
  // If there is no overlay UI displayed using presentation or containment, the
  // presentation context should be laid out with an empty frame to allow
  // touches to pass freely to the underlying browser UI.
  UIViewController* presentedViewController = self.presentedViewController;
  if (!presentedViewController) {
    return nil;
  }

  // If overlay UI is displayed using custom UIViewController presentation with
  // an OverlayPresentationController that resizes the container view, the
  // presentation context should match the presented view's container.
  if (self.presentedViewControllerResizesContainer) {
    return presentedViewController.presentationController.containerView;
  }

  // For all other UIViewController presentation, the context should be laid out
  // to match the presenter view.
  return self.presentingViewController.view;
}

@end
