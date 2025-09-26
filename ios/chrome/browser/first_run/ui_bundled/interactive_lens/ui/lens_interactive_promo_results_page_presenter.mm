// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/lens_interactive_promo_results_page_presenter.h"

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_results_page_presenter_delegate.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The vertical offset padding.
const CGFloat kSelectionOffsetPadding = 72.0;

// The preferred corner radius for the bottom sheet.
const CGFloat kPreferredCornerRadius = 14.0;

// The height of the bottom sheet as a multiplier of the container's height.
const CGFloat kBottomSheetHeightMultiplier = 0.5;

// The duration of the opacity transition in the results page.
const CGFloat kOpacityAnimationDuration = 0.4;

}  // namespace

@interface LensInteractivePromoResultsPagePresenter () <
    LensResultPageViewControllerDelegate>
@end

@implementation LensInteractivePromoResultsPagePresenter {
  /// The base on top of which the results view controller is presented.
  __weak LensOverlayContainerViewController* _baseViewController;

  /// The results view controller to present.
  __weak LensResultPageViewController* _resultViewController;

  /// The layout guide that defines the unobstructed area by the presentation.
  UILayoutGuide* _visibleAreaLayoutGuide;
}

@synthesize delegate = _delegate;

- (instancetype)initWithBaseViewController:
                    (LensOverlayContainerViewController*)baseViewController
                  resultPageViewController:
                      (LensResultPageViewController*)resultViewController {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _resultViewController = resultViewController;
    _visibleAreaLayoutGuide = [[UILayoutGuide alloc] init];
  }

  return self;
}

#pragma mark - LensOverlayResultsPagePresenting

- (BOOL)isResultPageVisible {
  return _resultViewController.parentViewController == _baseViewController;
}

- (SheetDimensionState)sheetDimension {
  return self.isResultPageVisible ? SheetDimensionState::kMedium
                                  : SheetDimensionState::kHidden;
}

- (CGFloat)presentedResultsPageHeight {
  return self.isResultPageVisible
             ? _baseViewController.view.bounds.size.height *
                   kBottomSheetHeightMultiplier
             : 0;
}

- (void)setDelegate:(id<LensOverlayResultsPagePresenterDelegate>)delegate {
  if (delegate != _delegate) {
    _delegate = delegate;
  }
}

- (void)presentResultsPageAnimated:(BOOL)animated
                     maximizeSheet:(BOOL)maximizeSheet
                  startInTranslate:(BOOL)startInTranslate
                        completion:(void (^)(void))completion {
  // The maximizeSheet and startInTranslate parameters are ignored in this
  // simple presentation.
  if (!_baseViewController || !_resultViewController ||
      !_baseViewController.view.window) {
    if (completion) {
      completion();
    }
    return;
  }

  [self resultsPagePresentationWillAppear];
  [self.interactivePromoDelegate
      lensInteractivePromoResultsPagePresenterWillPresentResults:self];

  auto presentationComplete = ^{
    if (completion) {
      completion();
    }
  };

  [self presentBottomSheetAnimated:animated completion:presentationComplete];
}

- (void)readjustPresentationIfNeeded {
  if (!self.isResultPageVisible) {
    return;
  }

  __weak __typeof(self) weakSelf = self;

  [self dismissResultsPageAnimated:NO
                        completion:^{
                          [weakSelf presentResultsPageAnimated:NO
                                                 maximizeSheet:NO
                                              startInTranslate:NO
                                                    completion:nil];
                        }];
}

- (void)dismissResultsPageAnimated:(BOOL)animated
                        completion:(void (^)(void))completion {
  if (![self isResultPageVisible]) {
    if (completion) {
      completion();
    }
    return;
  }

  __weak __typeof(self) weakSelf = self;
  void (^dismissalCompletion)(BOOL) = ^(BOOL finished) {
    LensInteractivePromoResultsPagePresenter* strongSelf = weakSelf;
    if (strongSelf) {
      [strongSelf->_resultViewController.view removeFromSuperview];
      [strongSelf->_resultViewController removeFromParentViewController];
      if (completion) {
        completion();
      }
    }
  };

  if (!animated) {
    dismissalCompletion(YES);
    return;
  }

  [UIView animateWithDuration:kOpacityAnimationDuration
                   animations:^{
                     LensInteractivePromoResultsPagePresenter* strongSelf =
                         weakSelf;
                     if (strongSelf) {
                       strongSelf->_resultViewController.view.transform =
                           CGAffineTransformMakeTranslation(
                               0, strongSelf->_baseViewController.view.bounds
                                      .size.height);
                     }
                   }
                   completion:dismissalCompletion];
}

#pragma mark - LensOverlayBottomSheetPresentationCommands

- (void)requestMaximizeBottomSheet {
  // No-op. Sheet size is constant in this presentation.
}

- (void)requestMinimizeBottomSheet {
  // No-op. Sheet size is constant in this presentation.
}

- (void)adjustForSelectionResult {
  [self adjustSelectionOcclusionInsets];
}

- (void)adjustForTranslateResult {
  // No-op.
}

- (void)hideBottomSheetWithCompletion:(void (^)(void))completion {
  [self dismissResultsPageAnimated:YES completion:completion];
  [self.interactivePromoDelegate
      lensInteractivePromoResultsPagePresenterDidDismissResults:self];
}

- (void)showInfoMessage:(LensOverlayBottomSheetInfoMessageType)infoMessageType {
  // No-op.
}

#pragma mark - LensResultPageViewControllerDelegate

- (void)lensResultPageViewControllerDidTapBottomSheetGrabber:
    (LensResultPageViewController*)lensResultPageViewController {
  // No-op. User interaction not supported for this presentation.
}

#pragma mark - Private

// Presents the results page as a simple, non-interactive bottom sheet.
- (void)presentBottomSheetAnimated:(BOOL)animated
                        completion:(void (^)(void))completion {
  // Add the child view controller and its view.
  [_baseViewController.view addSubview:_resultViewController.view];
  [_baseViewController addChildViewController:_resultViewController];
  _resultViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  _resultViewController.view.layer.cornerRadius = kPreferredCornerRadius;
  _resultViewController.view.layer.masksToBounds = YES;
  _resultViewController.view.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;

  // Activate the final layout constraints.
  AddSameConstraintsToSides(
      _resultViewController.view, _baseViewController.view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  [NSLayoutConstraint activateConstraints:@[
    [_resultViewController.view.heightAnchor
        constraintEqualToAnchor:_baseViewController.view.heightAnchor
                     multiplier:kBottomSheetHeightMultiplier],
  ]];

  [self adjustSelectionOcclusionInsets];
  [self setUpVisibleAreaLayoutGuideIfNeeded];

  // Before the animation, place the view off-screen.
  _resultViewController.view.transform = CGAffineTransformMakeTranslation(
      0, _baseViewController.view.bounds.size.height);

  // Animate the view to its final position.
  __weak __typeof(self) weakSelf = self;
  UIView* viewToAnimate = _resultViewController.view;
  [UIView animateWithDuration:kOpacityAnimationDuration
      animations:^{
        viewToAnimate.transform = CGAffineTransformIdentity;
      }
      completion:^(BOOL finished) {
        [weakSelf
            handlePresentationAnimationCompletedWithCompletion:completion];
      }];

  _resultViewController.view.userInteractionEnabled = NO;
}

// Sets up the layout guide that defines the area not obstructed by the results
// page.
- (void)setUpVisibleAreaLayoutGuideIfNeeded {
  if ([_baseViewController.view.layoutGuides
          containsObject:_visibleAreaLayoutGuide]) {
    return;
  }

  [_baseViewController.view addLayoutGuide:_visibleAreaLayoutGuide];
  AddSameConstraintsToSides(
      _visibleAreaLayoutGuide, _baseViewController.view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);
  [NSLayoutConstraint activateConstraints:@[
    [_visibleAreaLayoutGuide.bottomAnchor
        constraintEqualToAnchor:_baseViewController.view.bottomAnchor
                       constant:-(_baseViewController.view.bounds.size.height *
                                  kBottomSheetHeightMultiplier)],
  ]];

  [_delegate lensOverlayResultsPagePresenter:self
             didAdjustVisibleAreaLayoutGuide:_visibleAreaLayoutGuide];
}

// Handles the final steps after the presentation animation is completed.
- (void)handlePresentationAnimationCompletedWithCompletion:
    (void (^)(void))completion {
  [_resultViewController didMoveToParentViewController:_baseViewController];
  if (completion) {
    completion();
  }
}

// Calculates and communicates the vertical space that the results page
// occludes.
- (void)adjustSelectionOcclusionInsets {
  // Pad the offset by a small ammount to avoid having the bottom edge of the
  // selection overlapped over the sheet.
  CGFloat sheetHeight = _baseViewController.view.bounds.size.height *
                        kBottomSheetHeightMultiplier;
  CGFloat offsetNeeded = sheetHeight + kSelectionOffsetPadding;
  [_delegate lensOverlayResultsPagePresenter:self
               updateVerticalOcclusionOffset:offsetNeeded];
}

// Performs setup tasks immediately before the results page is presented.
- (void)resultsPagePresentationWillAppear {
  _resultViewController.delegate = self;
  _resultViewController.view.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  [_resultViewController setBottomSheetGrabberVisible:YES];
}

@end
