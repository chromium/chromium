// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_results_page_presenter.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_pan_tracker.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_presentation_type.h"
#import "ios/chrome/browser/lens_overlay/ui/info_message/lens_translate_error_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/info_message/lens_translate_indication_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_info_message_animator.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_results_page_presenter_delegate.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The vertical offset padding.
const CGFloat kSelectionOffsetPadding = 72.0f;

// The preferred corner radius for the bottom sheet.
const CGFloat kPreferredCornerRadius = 14.0;

// The maximum height of the bottom sheet before it automatically closes when
// released.
const CGFloat kThresholdHeightForClosingSheet = 200.0f;

// The threshold from the medium detent when the visible area can get obstructed
// by the bottom sheet presentation.
const CGFloat kVisibleAreaMediumDetentThreshold = 100.0f;

// The horizontal occlusion inset to apply when the side panel is displayed.
const CGFloat kSidePanelHorizontalOcclusionInset = 24.0f;

}  // namespace

@interface LensOverlayResultsPagePresenter () <
    LensOverlayPanTrackerDelegate,
    LensOverlayDetentsManagerDelegate,
    LensResultPageViewControllerDelegate,
    LensOverlayBottomSheetPresenterDelegate,
    UINavigationControllerDelegate>
@end

@implementation LensOverlayResultsPagePresenter {
  /// The base on top of which the results view controller is presented.
  __weak LensOverlayContainerViewController* _baseViewController;

  /// The results view controller to present.
  __weak LensResultPageViewController* _resultViewController;

  /// Orchestrates the change in detents of the associated bottom sheet.
  LensOverlayDetentsManager* _detentsManager;

  /// Used to monitor the results sheet position relative to the container.
  CADisplayLink* _displayLink;

  /// Tracks whether the user is currently touching the screen.
  LensOverlayPanTracker* _windowPanTracker;

  /// Tracks whether the user is currently the view behind the sheet
  LensOverlayPanTracker* _basePanTracker;

  /// The constraint corresponding to the bottom offset of the visible area.
  NSLayoutConstraint* _visibleAreaBottomConstraint;

  /// Whether the presenting animation is in progress.
  BOOL _presentingAnimationInProgress;

  // The layout guide that defines the unobstructed area by the presentation.
  UILayoutGuide* _visibleAreaLayoutGuide;

  // TODO(crbug.com/405199044): Consider using a custom container for the UI
  // orchestration.
  //
  // The navigation controller orchestrating the change between the results page
  // and the informational messages.
  UINavigationController* _presentationNavigationController;

  // Stores the height of the presented results page.
  CGFloat _presentedResultsPageHeight;
}

@synthesize delegate = _delegate;

- (instancetype)initWithBaseViewController:
                    (LensOverlayContainerViewController*)baseViewController
                  resultPageViewController:
                      (LensResultPageViewController*)resultViewController {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _baseViewController.bottomSheet.sheetDelegate = self;
    _resultViewController = resultViewController;
    _presentationNavigationController = [[UINavigationController alloc]
        initWithRootViewController:resultViewController];
    _presentationNavigationController.toolbarHidden = YES;
    _presentationNavigationController.navigationBarHidden = YES;
    _presentationNavigationController.delegate = self;

    _visibleAreaLayoutGuide = [[UILayoutGuide alloc] init];
  }

  return self;
}

- (BOOL)isResultPageVisible {
  if (_baseViewController.sidePanelPresented) {
    return YES;
  }

  if (UseCustomLensOverlayBottomSheet()) {
    return _baseViewController.bottomSheet.bottomSheetPresented;
  } else {
    return _baseViewController.presentedViewController != nil &&
           _baseViewController.presentedViewController ==
               _presentationNavigationController;
  }
}

- (SheetDimensionState)sheetDimension {
  if (!_detentsManager) {
    return SheetDimensionState::kHidden;
  }

  return _detentsManager.sheetDimension;
}

- (UIWindow*)presentationWindow {
  return _baseViewController.view.window;
}

- (CGFloat)presentedResultsPageHeight {
  if (UseCustomLensOverlayBottomSheet()) {
    return _baseViewController.bottomSheet.bottomSheetHeight;
  } else {
    return _presentedResultsPageHeight;
  }
}

- (void)setDelegate:(id<LensOverlayResultsPagePresenterDelegate>)delegate {
  if (delegate != _delegate) {
    _delegate = delegate;
    _displayLink.paused = delegate == nil;
  }
}

- (void)setUpVisibleAreaLayoutGuideIfNeeded {
  // If it's a reveal the layout guide it's already set up.
  if ([_baseViewController.view.layoutGuides
          containsObject:_visibleAreaLayoutGuide]) {
    return;
  }

  [_baseViewController.view addLayoutGuide:_visibleAreaLayoutGuide];

  _visibleAreaBottomConstraint = [_visibleAreaLayoutGuide.bottomAnchor
      constraintEqualToAnchor:_baseViewController.view.bottomAnchor];
  [NSLayoutConstraint activateConstraints:@[
    _visibleAreaBottomConstraint,
    [_visibleAreaLayoutGuide.topAnchor
        constraintEqualToAnchor:_baseViewController.view.topAnchor],
    [_visibleAreaLayoutGuide.leftAnchor
        constraintEqualToAnchor:_baseViewController.view.leftAnchor],
    [_visibleAreaLayoutGuide.rightAnchor
        constraintEqualToAnchor:_baseViewController.view.rightAnchor],
  ]];

  [_delegate lensOverlayResultsPagePresenter:self
             didAdjustVisibleAreaLayoutGuide:_visibleAreaLayoutGuide];
}

- (void)presentResultsPageAnimated:(BOOL)animated
                     maximizeSheet:(BOOL)maximizeSheet
                  startInTranslate:(BOOL)startInTranslate
                        completion:(void (^)(void))completion {
  if (!_baseViewController || !_resultViewController ||
      !self.presentationWindow) {
    if (completion) {
      completion();
    }
    return;
  }

  [self resultsPagePresentationWillAppear];

  __weak __typeof(self) weakSelf = self;
  auto presentationComplete = ^{
    [weakSelf resultsPagePresentationDidAppear];
    if (completion) {
      completion();
    }
  };

  BOOL presentInSidePanel =
      lens::ResultPagePresentationFor(_baseViewController) ==
      lens::ResultPagePresentationType::kSidePanel;
  if (presentInSidePanel) {
    [self presentSidePanelAnimated:animated completion:presentationComplete];
  } else {
    [self presentBottomSheetAnimated:animated
                       maximizeSheet:maximizeSheet
                    startInTranslate:startInTranslate
                          completion:presentationComplete];
  }
}

- (void)presentSidePanelAnimated:(BOOL)animated
                      completion:(void (^)(void))completion {
  [_baseViewController
      presentViewControllerInSidePanel:_presentationNavigationController
                              animated:animated
                            completion:completion];
}

- (void)presentBottomSheetAnimated:(BOOL)animated
                     maximizeSheet:(BOOL)maximizeSheet
                  startInTranslate:(BOOL)startInTranslate
                        completion:(void (^)(void))completion {
  SheetDetentPresentationStategy strategy =
      startInTranslate ? SheetDetentPresentationStategyTranslate
                       : SheetDetentPresentationStategySelection;
  [self setupDetentsManagerWithStrategy:strategy
                          maximizeSheet:maximizeSheet
                               animated:animated];
  // Adjust the occlusion insets so that selections in the bottom half of the
  // screen are repositioned, to avoid being hidden by the bottom sheet.
  //
  // Note(crbug.com/370930119): The adjustment has to be done right before the
  // bottom sheet is presented. Otherwise the coachmark will appear displaced.
  // This is a known limitation on the Lens side, as there is currently no
  // independent way of adjusting the insets for the coachmark alone.
  [self adjustSelectionOcclusionInsets];

  // If there is already a view controller presented (e.g. the overflow menu)
  // dismiss it before presenting.
  if (_baseViewController.presentedViewController) {
    [_baseViewController dismissViewControllerAnimated:NO completion:nil];
  }

  __weak __typeof(self) weakSelf = self;
  if (UseCustomLensOverlayBottomSheet()) {
    [_baseViewController
        presentViewControllerInBottomSheet:_presentationNavigationController
                                  animated:animated
                                completion:^{
                                  [weakSelf resultsPagePresentationDidAppear];
                                  if (completion) {
                                    completion();
                                  }
                                }];
  } else {
    [self setupGestureTrackers];
    [self setUpVisibleAreaLayoutGuideIfNeeded];
    [self monitorResultsBottomSheetPosition];

    // Presenting the bottom sheet adds a gesture recognizer on the main window
    // which in turn causes the touches on Lens Overlay to get canceled.
    // To prevent such a behavior, extract the recognizers added as a
    // consequence of presenting and allow touches to be delivered to views.
    __block NSSet<UIGestureRecognizer*>* panRecognizersBeforePresenting =
        [self panGestureRecognizersOnWindow];

    ProceduralBlock afterPresentation = ^{
      [weakSelf resultsPagePresentationDidAppear];
      [weakSelf handlePanRecognizersAddedAfter:panRecognizersBeforePresenting];
      if (completion) {
        completion();
      }
    };

    [_baseViewController presentViewController:_presentationNavigationController
                                      animated:animated
                                    completion:afterPresentation];
  }
}

- (void)readjustPresentationIfNeeded {
  if (!self.isResultPageVisible) {
    return;
  }

  BOOL isAlreadySidePanel = _baseViewController.sidePanelPresented;
  BOOL presentInSidePanel =
      lens::ResultPagePresentationFor(_baseViewController) ==
      lens::ResultPagePresentationType::kSidePanel;
  // Refrain from rebuilding the presentation there was no change in the
  // presentation type.
  BOOL shouldRebuildPresentation = isAlreadySidePanel ^ presentInSidePanel;
  if (!shouldRebuildPresentation) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  BOOL maximizeSheet =
      _detentsManager.sheetDimension == SheetDimensionState::kLarge;
  BOOL startInTranslate = _detentsManager.presentationStrategy ==
                          SheetDetentPresentationStategyTranslate;
  [self dismissResultsPageAnimated:NO
                        completion:^{
                          [weakSelf presentResultsPageAnimated:NO
                                                 maximizeSheet:maximizeSheet
                                              startInTranslate:startInTranslate
                                                    completion:nil];
                        }];
}

- (void)showInfoMessage:(LensOverlayBottomSheetInfoMessageType)infoMessageType {
  UIViewController* infoMessageViewController;
  switch (infoMessageType) {
    case LensOverlayBottomSheetInfoMessageType::kImageTranslatedIndication:
      infoMessageViewController =
          [[LensTranslateIndicationViewController alloc] init];
      break;
    case LensOverlayBottomSheetInfoMessageType::kNoTranslatableTextWarning:
      infoMessageViewController =
          [[LensTranslateErrorViewController alloc] init];
      break;
  }

  _detentsManager.infoMessageHeight =
      infoMessageViewController.preferredContentSize.height;

  [_detentsManager adjustDetentsForState:SheetDetentStateInfoMessage];
  [_presentationNavigationController
      pushViewController:infoMessageViewController
                animated:YES];
}

- (void)hideBottomSheetWithCompletion:(void (^)(void))completion {
  [self resultsPagePresentationWillDismiss];

  if (UseCustomLensOverlayBottomSheet()) {
    [_baseViewController dismissBottomSheetAnimated:YES completion:completion];
  } else {
    UIViewController* presentedVC = _baseViewController.presentedViewController;
    [presentedVC dismissViewControllerAnimated:YES completion:completion];
  }
}

- (void)dismissResultsPageAnimated:(BOOL)animated
                        completion:(void (^)(void))completion {
  [self resultsPagePresentationWillDismiss];

  if (_baseViewController.sidePanelPresented) {
    [_baseViewController dismissSidePanelAnimated:animated
                                       completion:completion];
    return;
  }

  if (UseCustomLensOverlayBottomSheet()) {
    if (_baseViewController.bottomSheet.bottomSheetPresented) {
      [_baseViewController dismissBottomSheetAnimated:animated
                                           completion:completion];
      return;
    }
  } else {
    UIViewController* presentedVC = _baseViewController.presentedViewController;
    if (!presentedVC) {
      if (completion) {
        completion();
      }
      return;
    }

    [presentedVC dismissViewControllerAnimated:animated completion:completion];
  }
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

  UIView* presentedView = _presentationNavigationController.view;

  // Early return if the bottom sheet is not displayed yet.
  CALayer* presentationLayer = presentedView.layer.presentationLayer;
  if (!presentationLayer) {
    return;
  }

  CGRect presentedFrame = presentedView.frame;
  CGRect newFrame = [presentedView convertRect:presentedFrame
                                        toView:_baseViewController.view];
  CGFloat containerHeight = _baseViewController.view.frame.size.height;
  CGFloat currentSheetHeight = containerHeight - newFrame.origin.y;

  // To keep in sync external animation, emply the approximated layer that is
  // currently being displayed onscreen as it accurately keeps track of the
  // bottom sheet positon throughout the animations (e.g. when the user finishes
  // dragging the sheet).
  CGRect presentationLayerConvertedFrame =
      [presentationLayer convertRect:presentationLayer.frame
                             toLayer:_baseViewController.view.layer];
  CGFloat presentationLayerHeight =
      containerHeight - presentationLayerConvertedFrame.origin.y;

  [self sheetPresentationHeightChanged:presentationLayerHeight];

  // Trigger the Lens UI exit flow when the release occurs below the threshold,
  // allowing the overlay animation to run concurrently with the sheet dismissal
  // one.
  BOOL sheetClosedThresholdReached =
      currentSheetHeight <= kThresholdHeightForClosingSheet;
  BOOL userTouchesTheScreen = _windowPanTracker.isPanning;

  BOOL infoMessageShown =
      _detentsManager.sheetDimension == SheetDimensionState::kInfoMessage;
  BOOL shouldDestroyLensUI =
      sheetClosedThresholdReached && !userTouchesTheScreen &&
      !_presentingAnimationInProgress && !infoMessageShown;
  if (shouldDestroyLensUI) {
    [_displayLink invalidate];
    [self sheetPresentationHeightChanged:0];
    [_windowPanTracker stopTracking];
    [self.delegate
        lensOverlayResultsPagePresenterWillInitiateGestureDrivenDismiss:self];
  }
}

- (void)sheetPresentationHeightChanged:(CGFloat)sheetHeight {
  CGFloat estimatedMediumDetentHeight =
      _detentsManager.estimatedMediumDetentHeight;
  CGFloat maximumOffset =
      estimatedMediumDetentHeight + kVisibleAreaMediumDetentThreshold;
  CGFloat bottomOffset = MIN(sheetHeight, maximumOffset);

  _visibleAreaBottomConstraint.constant = -bottomOffset;

  _presentedResultsPageHeight = sheetHeight;
}

- (void)adjustSelectionOcclusionInsets {
  // Update the horizontal padding for side panel display.
  if (_baseViewController.sidePanelPresented) {
    [_delegate
        lensOverlayResultsPagePresenter:self
        updateHorizontalOcclusionOffset:kSidePanelHorizontalOcclusionInset];
    return;
  }

  // Pad the offset by a small ammount to avoid having the bottom edge of the
  // selection overlapped over the sheet.
  CGFloat estimatedMediumDetentHeight =
      _detentsManager.estimatedMediumDetentHeight;
  CGFloat offsetNeeded = estimatedMediumDetentHeight + kSelectionOffsetPadding;
  [_delegate lensOverlayResultsPagePresenter:self
               updateVerticalOcclusionOffset:offsetNeeded];
}

#pragma mark - Presentation lifecycle

// Called before the results page is presented.
- (void)resultsPagePresentationWillAppear {
  _presentingAnimationInProgress = YES;
  _resultViewController.delegate = self;
  _presentationNavigationController.view.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  BOOL presentedInBottomSheet =
      lens::ResultPagePresentationFor(_baseViewController) ==
      lens::ResultPagePresentationType::kEdgeAttachedBottomSheet;
  [_resultViewController setOmniboxEnabled:YES];
  [_resultViewController setBottomSheetGrabberVisible:presentedInBottomSheet];
}

// Called after the results page has appeared.
- (void)resultsPagePresentationDidAppear {
  _presentingAnimationInProgress = NO;
}

// Called before the results page is dismissed.
- (void)resultsPagePresentationWillDismiss {
  if (!UseCustomLensOverlayBottomSheet()) {
    [_displayLink invalidate];
    [self sheetPresentationHeightChanged:0];
    [_windowPanTracker stopTracking];
    [_basePanTracker stopTracking];
  }
  _detentsManager = nil;
}

// Sets up the required gesture trackers for the bottom sheet presentation.
- (void)setupGestureTrackers {
  _windowPanTracker =
      [[LensOverlayPanTracker alloc] initWithView:self.presentationWindow];
  _windowPanTracker.delegate = self;
  [_windowPanTracker startTracking];

  _basePanTracker =
      [[LensOverlayPanTracker alloc] initWithView:_baseViewController.view];
  [_basePanTracker startTracking];
}

// Sets up the detents manager for the bottom sheet presentation.
- (void)setupDetentsManagerWithStrategy:(SheetDetentPresentationStategy)strategy
                          maximizeSheet:(BOOL)maximizeSheet
                               animated:(BOOL)animated {
  if (UseCustomLensOverlayBottomSheet()) {
    _detentsManager = [[LensOverlayDetentsManager alloc]
        initWithLensOverlayBottomSheet:_baseViewController.bottomSheet
                                window:self.presentationWindow
                  presentationStrategy:strategy];
  } else {
    UISheetPresentationController* sheet =
        _presentationNavigationController.sheetPresentationController;
    sheet.prefersEdgeAttachedInCompactHeight = YES;
    sheet.preferredCornerRadius = kPreferredCornerRadius;
    _detentsManager = [[LensOverlayDetentsManager alloc]
         initWithBottomSheet:sheet
                      window:self.presentationWindow
        presentationStrategy:strategy];
  }

  _detentsManager.delegate = self;
  [_detentsManager adjustDetentsForState:SheetDetentStateUnrestrictedMovement
                                animated:animated];

  if (maximizeSheet) {
    [_detentsManager requestMaximizeBottomSheet];
  }
}

#pragma mark - UIPanGestureRecognizer handlers

- (NSSet<UIPanGestureRecognizer*>*)panGestureRecognizersOnWindow {
  NSMutableSet<UIPanGestureRecognizer*>* panRecognizersOnWindow =
      [[NSMutableSet alloc] init];

  if (!self.presentationWindow) {
    return panRecognizersOnWindow;
  }

  for (UIGestureRecognizer* recognizer in self.presentationWindow
           .gestureRecognizers) {
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
    (NSSet<UIGestureRecognizer*>*)panRecognizersBeforePresenting {
  NSMutableSet<UIGestureRecognizer*>* panRecognizersAfterPresenting =
      [[self panGestureRecognizersOnWindow] mutableCopy];
  [panRecognizersAfterPresenting minusSet:panRecognizersBeforePresenting];
  for (UIGestureRecognizer* recognizer in panRecognizersAfterPresenting) {
    recognizer.cancelsTouchesInView = NO;
  }
}

#pragma mark - LensOverlayPanTrackerDelegate

- (void)lensOverlayPanTracker:(LensOverlayPanTracker*)panTracker
    didEndPanGestureWithVelocity:(CGPoint)velocity {
  if (panTracker != _windowPanTracker) {
    return;
  }

  // Keep peaking only for the duration of the gesture.
  if (_detentsManager.sheetDimension == SheetDimensionState::kPeaking) {
    [_detentsManager
        adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
  }
}

#pragma mark - LensOverlayDetentsManagerDelegate

- (void)lensOverlayDetentsManagerDidChangeDimensionState:
    (LensOverlayDetentsManager*)detentsManager {
  [_delegate lensOverlayResultsPagePresenter:self
                     didUpdateDimensionState:detentsManager.sheetDimension];
}

- (BOOL)lensOverlayDetentsManagerShouldDismissBottomSheet:
    (LensOverlayDetentsManager*)detentsManager {
  switch (self.sheetDimension) {
    case SheetDimensionState::kConsent:
    case SheetDimensionState::kHidden:
      return YES;
    case SheetDimensionState::kPeaking:
    case SheetDimensionState::kInfoMessage:
    case SheetDimensionState::kLarge:
      return NO;
    case SheetDimensionState::kMedium:
      // If the user is actively adjusting a selection (by moving the selection
      // frame), it means the sheet dismissal was incidental and shouldn't be
      // processed. Only when the sheet is directly dragged downwards should the
      // dismissal intent be considered.
      if (!UseCustomLensOverlayBottomSheet()) {
        if (_basePanTracker.isPanning) {
          // Instead, when a touch collision is detected, go into the peak
          // state.
          [_detentsManager adjustDetentsForState:SheetDetentStatePeakEnabled];
          return NO;
        }
      }

      return YES;
  }
}

#pragma mark - LensOverlayBottomSheetPresenterDelegate

- (void)lensOverlayBottomSheetWillPresent:
    (id<LensOverlayBottomSheet>)bottomSheet {
  if ([_baseViewController.view.layoutGuides
          containsObject:_visibleAreaLayoutGuide]) {
    return;
  }

  [_baseViewController.view addLayoutGuide:_visibleAreaLayoutGuide];

  AddSameConstraintsToSides(
      _visibleAreaLayoutGuide,
      _baseViewController.bottomSheet.visibleAreaLayoutGuide,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);

  CGFloat estimatedMediumDetentHeight =
      _detentsManager.estimatedMediumDetentHeight;
  CGFloat maximumOffset =
      estimatedMediumDetentHeight + kVisibleAreaMediumDetentThreshold;

  // Track the bottom sheet until it reaches a target height, then pin the
  // visible area to prevent further scrolling while the sheet continues to move
  // over other elements.
  NSLayoutConstraint* anchoredToSheetBottom =
      [_visibleAreaLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_baseViewController.bottomSheet
                                      .visibleAreaLayoutGuide.bottomAnchor];
  NSLayoutConstraint* maximumMovement = [_visibleAreaLayoutGuide.bottomAnchor
      constraintGreaterThanOrEqualToAnchor:_baseViewController.view.bottomAnchor
                                  constant:-maximumOffset];
  maximumMovement.priority = UILayoutPriorityRequired;
  anchoredToSheetBottom.priority = UILayoutPriorityRequired - 1;

  [NSLayoutConstraint
      activateConstraints:@[ anchoredToSheetBottom, maximumMovement ]];

  [_delegate lensOverlayResultsPagePresenter:self
             didAdjustVisibleAreaLayoutGuide:_visibleAreaLayoutGuide];
}

- (void)lensOverlayBottomSheetWillDismiss:
            (id<LensOverlayBottomSheet>)bottomSheet
                            gestureDriven:(BOOL)gestureDriven {
  [_resultViewController setOmniboxEnabled:NO];
}

- (void)lensOverlayBottomSheetDidDismiss:(id<LensOverlayBottomSheet>)bottomSheet
                           gestureDriven:(BOOL)userInitiated {
  if (userInitiated) {
    [_delegate lensOverlayResultsPagePresenter:self
                       didUpdateDimensionState:SheetDimensionState::kHidden];
  }
}

- (NSArray<UIView*>*)lensOverlayBottomSheetAttachedViews:
    (id<LensOverlayBottomSheet>)bottomSheet {
  // The base view controller contains internal elements that are dependent of
  // the position of the bottom sheet
  return @[ _baseViewController.view ];
}

- (void)lensOverlayBottomSheet:(id<LensOverlayBottomSheet>)bottomSheet
    animateAttachedUIDismissWithCompletion:(ProceduralBlock)completion {
  if (!self.delegate) {
    if (completion) {
      completion();
      return;
    }
  }
  [self.delegate lensOverlayResultsPagePresenter:self
          animateAttachedUIDismissWithCompletion:completion];
}

#pragma mark - LensOverlayBottomSheetPresentationCommands

// Request resizing the bottom sheet to maximum size.
- (void)requestMaximizeBottomSheet {
  [_detentsManager requestMaximizeBottomSheet];
}

// Request resizing the bottom sheet to minimum size.
- (void)requestMinimizeBottomSheet {
  [_detentsManager requestMinimizeBottomSheet];
}

- (void)adjustForSelectionResult {
  _detentsManager.presentationStrategy =
      SheetDetentPresentationStategySelection;
  [_presentationNavigationController popToRootViewControllerAnimated:YES];
  [_baseViewController.view layoutIfNeeded];
  [self adjustSelectionOcclusionInsets];
}

- (void)adjustForTranslateResult {
  _detentsManager.presentationStrategy =
      SheetDetentPresentationStategyTranslate;
  [_presentationNavigationController popToRootViewControllerAnimated:YES];
  [_baseViewController.view layoutIfNeeded];
  [self adjustSelectionOcclusionInsets];
}

#pragma mark - UINavigationControllerDelegate

- (id<UIViewControllerAnimatedTransitioning>)
               navigationController:
                   (UINavigationController*)navigationController
    animationControllerForOperation:(UINavigationControllerOperation)operation
                 fromViewController:(UIViewController*)fromVC
                   toViewController:(UIViewController*)toVC {
  return [[LensOverlayInfoMessageAnimator alloc] initWithOperation:operation];
}

- (void)lensResultPageViewControllerDidTapBottomSheetGrabber:
    (LensResultPageViewController*)lensResultPageViewController {
  if (_detentsManager.sheetDimension == SheetDimensionState::kMedium) {
    [self requestMaximizeBottomSheet];
    return;
  }

  if (_detentsManager.sheetDimension == SheetDimensionState::kLarge) {
    [self requestMinimizeBottomSheet];
    return;
  }
}

@end
