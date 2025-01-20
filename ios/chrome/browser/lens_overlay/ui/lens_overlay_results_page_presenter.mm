// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_results_page_presenter.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_pan_tracker.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_results_page_presenter_delegate.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

namespace {

// THe vertical offset padding
const CGFloat kSelectionOffsetPadding = 100.0f;

// The preferred corner radius for the bottom sheet.
const CGFloat kPreferredCornerRadius = 14.0;

// The maximum height of the bottom sheet before it automatically closes when
// released.
const CGFloat kThresholdHeightForClosingSheet = 200.0f;

}  // namespace

@interface LensOverlayResultsPagePresenter () <LensOverlayPanTrackerDelegate,
                                               LensOverlayDetentsChangeObserver>
@end

@implementation LensOverlayResultsPagePresenter {
  /// The base on top of which the results view controller is presented.
  __weak UIViewController* _baseViewController;

  /// The results view controller to present.
  __weak UIViewController* _resultViewController;

  /// Orchestrates the change in detents of the associated bottom sheet.
  LensOverlayDetentsManager* _detentsManager;

  /// Used to monitor the results sheet position relative to the container.
  CADisplayLink* _displayLink;

  /// Tracks whether the user is currently touching the screen.
  LensOverlayPanTracker* _windowPanTracker;

  /// Tracks whether the user is currently the view behind the sheet
  LensOverlayPanTracker* _basePanTracker;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                  resultPageViewController:
                      (LensResultPageViewController*)resultViewController {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _resultViewController = resultViewController;
  }

  return self;
}

- (BOOL)isResultPageVisible {
  return _baseViewController.presentedViewController != nil &&
         _baseViewController.presentedViewController == _resultViewController;
}

- (SheetDimensionState)sheetDimension {
  if (!_detentsManager) {
    return SheetDimensionStateHidden;
  }

  return _detentsManager.sheetDimension;
}

- (void)setDelegate:(id<LensOverlayResultsPagePresenterDelegate>)delegate {
  if (delegate != _delegate) {
    _delegate = delegate;
    _displayLink.paused = delegate == nil;
  }
}

- (void)presentResultsPageAnimated:(BOOL)animated
                        sceneState:(SceneState*)sceneState
                     maximizeSheet:(BOOL)maximizeSheet
                        completion:(void (^)(void))completion {
  __weak UIWindow* window = sceneState.window;
  if (!_baseViewController || !_resultViewController || !window) {
    if (completion) {
      completion();
    }
    return;
  }

  UISheetPresentationController* sheet =
      _resultViewController.sheetPresentationController;
  sheet.prefersEdgeAttachedInCompactHeight = YES;
  sheet.prefersGrabberVisible = YES;
  sheet.preferredCornerRadius = kPreferredCornerRadius;

  _windowPanTracker = [[LensOverlayPanTracker alloc] initWithView:window];
  _windowPanTracker.delegate = self;
  [_windowPanTracker startTracking];

  _basePanTracker =
      [[LensOverlayPanTracker alloc] initWithView:_baseViewController.view];
  [_basePanTracker startTracking];

  _detentsManager =
      [[LensOverlayDetentsManager alloc] initWithBottomSheet:sheet];
  _detentsManager.observer = self;
  [_detentsManager adjustDetentsForState:SheetDetentStateUnrestrictedMovement];

  if (maximizeSheet) {
    [_detentsManager requestMaximizeBottomSheet];
  }

  // Adjust the occlusion insets so that selections in the bottom half of the
  // screen are repositioned, to avoid being hidden by the bottom sheet.
  //
  // Note(crbug.com/370930119): The adjustment has to be done right before the
  // bottom sheet is presented. Otherwise the coachmark will appear displaced.
  // This is a known limitation on the Lens side, as there is currently no
  // independent way of adjusting the insets for the coachmark alone.
  [self adjustSelectionOcclusionInsetsForWindow:window];

  // Presenting the bottom sheet adds a gesture recognizer on the main window
  // which in turn causes the touches on Lens Overlay to get canceled.
  // To prevent such a behavior, extract the recognizers added as a consequence
  // of presenting and allow touches to be delivered to views.
  __block NSSet<UIGestureRecognizer*>* panRecognizersBeforePresenting =
      [self panGestureRecognizersOnWindow:window];

  __weak __typeof(self) weakSelf = self;
  [_baseViewController
      presentViewController:_resultViewController
                   animated:animated
                 completion:^{
                   [weakSelf monitorResultsBottomSheetPosition];
                   [weakSelf handlePanRecognizersAddedAfter:
                                 panRecognizersBeforePresenting
                                                   onWindow:window];
                   if (completion) {
                     completion();
                   }
                 }];
}

- (void)dismissResultsPageAnimated:(BOOL)animated
                        completion:(void (^)(void))completion {
  [_displayLink invalidate];
  [_windowPanTracker stopTracking];
  [_basePanTracker stopTracking];

  UIViewController* presentedVC = _baseViewController.presentedViewController;
  if (!presentedVC) {
    if (completion) {
      completion();
    }
    return;
  }

  [presentedVC dismissViewControllerAnimated:animated completion:completion];
  _resultViewController = nil;
}

- (void)monitorResultsBottomSheetPosition {
  // Currently there is no system API for reactively obtaining the position of a
  // bottom sheet. For the lifetime of the LRP, use the display link to monitor
  // the position of it's frame relative to the container.

  // Invalidate any pre-existing display link before creating a new one.
  [_displayLink invalidate];
  _displayLink =
      [CADisplayLink displayLinkWithTarget:self
                                  selector:@selector(onDisplayLinkUpdate:)];
  [_displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                     forMode:NSRunLoopCommonModes];
  _displayLink.paused = _delegate == nil;
}

- (void)onDisplayLinkUpdate:(CADisplayLink*)sender {
  if (!_baseViewController || !_resultViewController) {
    return;
  }

  CGRect presentedFrame = _resultViewController.view.frame;
  CGRect newFrame =
      [_resultViewController.view convertRect:presentedFrame
                                       toView:_baseViewController.view];
  CGFloat containerHeight = _baseViewController.view.frame.size.height;
  CGFloat currentSheetHeight = containerHeight - newFrame.origin.y;

  // Trigger the Lens UI exit flow when the release occurs below the threshold,
  // allowing the overlay animation to run concurrently with the sheet dismissal
  // one.
  BOOL sheetClosedThresholdReached =
      currentSheetHeight <= kThresholdHeightForClosingSheet;
  BOOL userTouchesTheScreen = _windowPanTracker.isPanning;
  BOOL shouldDestroyLensUI =
      sheetClosedThresholdReached && !userTouchesTheScreen;
  if (shouldDestroyLensUI) {
    [_displayLink invalidate];
    [_windowPanTracker stopTracking];
    [self.delegate onResultsPageWillInitiateGestureDrivenDismiss];
  }
}

- (void)adjustSelectionOcclusionInsetsForWindow:(UIWindow*)window {
  // Pad the offset by a small ammount to avoid having the bottom edge of the
  // selection overlapped over the sheet.
  CGFloat estimatedMediumDetentHeight = window.frame.size.height / 2;
  CGFloat offsetNeeded = estimatedMediumDetentHeight + kSelectionOffsetPadding;

  [self.delegate onResultsPageVerticalOcclusionInsetsSettled:offsetNeeded];
}

#pragma mark - UIPanGestureRecognizer handlers

- (NSSet<UIPanGestureRecognizer*>*)panGestureRecognizersOnWindow:
    (UIWindow*)window {
  NSMutableSet<UIPanGestureRecognizer*>* panRecognizersOnWindow =
      [[NSMutableSet alloc] init];

  if (!window) {
    return panRecognizersOnWindow;
  }

  for (UIGestureRecognizer* recognizer in window.gestureRecognizers) {
    if (recognizer &&
        [recognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
      [panRecognizersOnWindow addObject:(UIPanGestureRecognizer*)recognizer];
    }
  }

  return panRecognizersOnWindow;
}

// Allow touches from gesture recognizers added by UIKit as a consequence of
// presenting a view controller.
- (void)handlePanRecognizersAddedAfter:
            (NSSet<UIGestureRecognizer*>*)panRecognizersBeforePresenting
                              onWindow:(UIWindow*)window {
  NSMutableSet<UIGestureRecognizer*>* panRecognizersAfterPresenting =
      [[self panGestureRecognizersOnWindow:window] mutableCopy];
  [panRecognizersAfterPresenting minusSet:panRecognizersBeforePresenting];
  for (UIGestureRecognizer* recognizer in panRecognizersAfterPresenting) {
    recognizer.cancelsTouchesInView = NO;
  }
}

#pragma mark - LensOverlayPanTrackerDelegate

- (void)onPanGestureStarted:(LensOverlayPanTracker*)tracker {
  // NO-OP
}

- (void)onPanGestureEnded:(LensOverlayPanTracker*)tracker {
  if (tracker == _windowPanTracker) {
    // Keep peaking only for the duration of the gesture.
    if (_detentsManager.sheetDimension == SheetDimensionStatePeaking) {
      [_detentsManager
          adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
    }
  }
}

#pragma mark - LensOverlayDetentsChangeObserver

- (void)onBottomSheetDimensionStateChanged:(SheetDimensionState)state {
  [self.delegate onResultsPageDimensionStateChanged:state];
}

- (BOOL)bottomSheetShouldDismissFromState:(SheetDimensionState)state {
  switch (state) {
    case SheetDimensionStateConsent:
    case SheetDimensionStateHidden:
      return YES;
    case SheetDimensionStatePeaking:
    case SheetDimensionStateLarge:
      return NO;
    case SheetDimensionStateMedium:
      // If the user is actively adjusting a selection (by moving the selection
      // frame), it means the sheet dismissal was incidental and shouldn't be
      // processed. Only when the sheet is directly dragged downwards should the
      // dismissal intent be considered.
      if (_basePanTracker.isPanning) {
        // Instead, when a touch collision is detected, go into the peak state.
        [_detentsManager adjustDetentsForState:SheetDetentStatePeakEnabled];
        return NO;
      }
      return YES;
  }
}

#pragma mark - LensOverlayBottomSheetPresentationDelegate

// Request resizing the bottom sheet to maximum size.
- (void)requestMaximizeBottomSheet {
  [_detentsManager requestMaximizeBottomSheet];
}

// Request resizing the bottom sheet to minimum size.
- (void)requestMinimizeBottomSheet {
  [_detentsManager requestMinimizeBottomSheet];
}

@end
