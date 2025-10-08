// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_bottom_sheet_view_controller.h"

#import <WebKit/WebKit.h>

#import <algorithm>
#import <ostream>
#import <utility>

#import "base/ios/block_types.h"
#import "base/notreached.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_pan_tracker.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_panel.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The threshold that determines when the close sequence should start.
const CGFloat kCloseBottomSheetThreshold = 200;

// The minimum display height of the bottom sheet. Below this value, the bottom
// sheet will start sliding down instead of resizing.
const CGFloat kBottomSheetMinimumDisplayHeight = 100;

// The deceleration of the outstanding sheet's motion just past its outermost
// top (and optionally bottom) detent is modeled by a logarithmic growth
// function that correlates the pan's vertical translation data with the actual
// displacement of the sheet.
//
// This coefficient controls the rate of logarithmic growth; lowering it makes
// the deceleration appear more aggressive.
const CGFloat kRubberBandCoefficient = 8.0;

// Approximate average time until the bottom sheet settles after user release.
const CGFloat kTimeToSteadyStateAfterRelease = 0.3;

// The increase caused by the keyboard beign shown, in the largest detent.
const CGFloat kKeyboardSheetHeightIncrease = 40.0;

// The identifier of the default detent.
NSString* const kDefaultDetentIdentifier = @"kDefaultDetentIdentifier";

// Whether the keyboard is currently shown.
BOOL _keyboardShown;

}  // namespace

// Delegates the touch response to another responder.
@protocol ViewTouchDelegate

// Indicates whether the given view contains the specified point.
- (BOOL)respondToPointInside:(CGPoint)point
                   withEvent:(UIEvent*)event
                     forView:(UIView*)view;

@end

// View for which the touch transparecy can be controlled by a different
@interface TouchDelegatingView : UIView

// Delegate for this instance.
@property(nonatomic, weak) id<ViewTouchDelegate> delegate;

@end

@implementation TouchDelegatingView

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  return [_delegate respondToPointInside:point withEvent:event forView:self];
}

@end

@interface LensOverlayBottomSheetViewController () <
    LensOverlayPanTrackerDelegate,
    ViewTouchDelegate>

// The current height of the bottom sheet, in points.
@property(nonatomic) CGFloat bottomSheetHeight;

// Tracks the pan inside the bottom sheet.
@property(nonatomic, readonly) LensOverlayPanTracker* panTracker;

// Constraints for handling the bottom sheet vertical position.
@property(nonatomic, strong) NSLayoutConstraint* bottomSheetHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* bottomSheetBottomConstraint;

@end

@implementation LensOverlayBottomSheetViewController {
  // An array of unowned scroll views, modeled through provider blocks that
  // capture weak references to the views in question.
  NSMutableArray<UIScrollView* (^)()>* _disabledScrollViews;

  // The bottom sheet container for the results page.
  LensOverlayPanel* _bottomSheet;

  // The default detent to be used if the list of detents was not overridden.
  LensOverlayBottomSheetDetent* _defaultDetent;

  // The bottom constraint of the layout guide tracking the unobstructed area.
  NSLayoutConstraint* _visibleAreaBottomConstraint;
}

@synthesize selectedDetentIdentifier = _selectedDetentIdentifier;
@synthesize detentsDelegate = _detentsDelegate;
@synthesize sheetDelegate = _sheetDelegate;
@synthesize detents = _detents;
@synthesize visibleAreaLayoutGuide = _visibleAreaLayoutGuide;

- (instancetype)init {
  self = [super init];
  if (self) {
    TouchDelegatingView* view = [[TouchDelegatingView alloc] init];
    self.view = view;
    view.delegate = self;
    _disabledScrollViews = [[NSMutableArray alloc] init];

    __weak __typeof(self) weakSelf = self;
    _visibleAreaLayoutGuide = [[UILayoutGuide alloc] init];

    _defaultDetent = [[LensOverlayBottomSheetDetent alloc]
        initWithIdentifier:kDefaultDetentIdentifier
             valueResolver:^{
               return [weakSelf bottomSheetContainerHeight] / 2;
             }];

    _detents = @[ _defaultDetent ];
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setupVisibleAreaLayoutGuideIfNeeded];
}

- (void)setupVisibleAreaLayoutGuideIfNeeded {
  BOOL alreadyAdded =
      [self.view.layoutGuides containsObject:_visibleAreaLayoutGuide];
  if (alreadyAdded) {
    return;
  }
  [self.view addLayoutGuide:_visibleAreaLayoutGuide];
  AddSameConstraintsToSides(
      _visibleAreaLayoutGuide, self.view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);

  [self constraintVisibleAreaBottomSheetTo:self.view.bottomAnchor];
}

- (void)setDetents:(NSArray<LensOverlayBottomSheetDetent*>*)detents {
  LensOverlayBottomSheetDetent* currentDetent =
      [self bottomSheetRestDetentWithVelocity:0];
  _detents = detents.count > 0 ? [detents copy] : @[ _defaultDetent ];

  LensOverlayBottomSheetDetent* nextClosestDetent =
      [self bottomSheetRestDetentWithVelocity:0];

  if (currentDetent.identifier == nextClosestDetent.identifier) {
    return;
  }

  [self setSelectedDetentIdentifier:nextClosestDetent.identifier animated:YES];
}

- (void)setContent:(UIViewController*)contentViewController {
  _bottomSheet = [[LensOverlayPanel alloc] initWithContent:contentViewController
                                              insetContent:NO];
}

- (BOOL)isBottomSheetPresented {
  return _bottomSheet != nil;
}

- (void)presentAnimated:(BOOL)animated completion:(ProceduralBlock)completion {
  [self setupVisibleAreaLayoutGuideIfNeeded];
  [_bottomSheet willMoveToParentViewController:self];
  _bottomSheet.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_bottomSheet];
  [self.view addSubview:_bottomSheet.view];
  [_bottomSheet didMoveToParentViewController:self];

  AddSameConstraintsToSides(_bottomSheet.view, self.view,
                            (LayoutSides::kLeading | LayoutSides::kTrailing));

  self.bottomSheetHeightConstraint =
      [_bottomSheet.view.heightAnchor constraintEqualToConstant:0];
  self.bottomSheetBottomConstraint = [_bottomSheet.view.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    self.bottomSheetHeightConstraint,
    self.bottomSheetBottomConstraint,
  ]];

  [self constraintVisibleAreaBottomSheetTo:_bottomSheet.view.topAnchor];

  [self.view layoutIfNeeded];

  _panTracker = [[LensOverlayPanTracker alloc] initWithView:_bottomSheet.view];
  _panTracker.cancelsTouchesInView = YES;
  _panTracker.delegate = self;

  CGFloat restHeight = [self bottomSheetRestHeight];

  [self.sheetDelegate lensOverlayBottomSheetWillPresent:self];
  if (!animated) {
    [self.panTracker startTracking];
    self.bottomSheetHeightConstraint.constant = restHeight;
    if (completion) {
      completion();
    }
    return;
  }

  [self animateBottomSheetHeight:restHeight
                      completion:^{
                        [self bottomSheetDidPresent];
                        if (completion) {
                          completion();
                        }
                      }];
}

- (void)bottomSheetDidPresent {
  [self.panTracker startTracking];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillHide:)
             name:UIKeyboardWillHideNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];
}

- (void)dismissAnimated:(BOOL)animated completion:(ProceduralBlock)completion {
  [self dismissAnimated:animated gestureDriven:NO completion:completion];
}

- (void)dismissAnimated:(BOOL)animated
          gestureDriven:(BOOL)gestureDriven
             completion:(ProceduralBlock)completion {
  [_panTracker stopTracking];

  [self.sheetDelegate lensOverlayBottomSheetWillDismiss:self
                                          gestureDriven:gestureDriven];

  __weak __typeof(self) weakSelf = self;

  if (!animated) {
    weakSelf.bottomSheetHeight = 0;
    [weakSelf bottomSheetDidDismissWithCompletion:completion
                                    gestureDriven:gestureDriven];
    return;
  }

  __block int completionCallCount = self.sheetDelegate ? 2 : 1;
  __block int completionCount = 0;
  void (^postDismiss)() = ^{
    completionCount++;
    if (completionCount == completionCallCount) {
      [weakSelf bottomSheetDidDismissWithCompletion:completion
                                      gestureDriven:gestureDriven];
    }
  };

  [self.sheetDelegate lensOverlayBottomSheet:self
      animateAttachedUIDismissWithCompletion:postDismiss];
  [self animateBottomSheetHeight:0 completion:postDismiss];
}

- (void)bottomSheetDidDismissWithCompletion:(ProceduralBlock)completion
                              gestureDriven:(BOOL)gestureDriven {
  [_bottomSheet willMoveToParentViewController:nil];
  [_bottomSheet.view removeFromSuperview];
  [_bottomSheet removeFromParentViewController];
  [_bottomSheet didMoveToParentViewController:nil];

  [self constraintVisibleAreaBottomSheetTo:self.view.bottomAnchor];

  [self.sheetDelegate lensOverlayBottomSheetDidDismiss:self
                                         gestureDriven:gestureDriven];
  _bottomSheet = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  if (completion) {
    completion();
  }
}

- (void)setSelectedDetentIdentifier:(NSString*)selectedDetentIdentifier
                           animated:(BOOL)animated {
  _selectedDetentIdentifier = selectedDetentIdentifier;
  CGFloat restHeight = [self bottomSheetRestHeight];
  if (!animated) {
    self.bottomSheetHeight = restHeight;
    return;
  }

  [_panTracker stopTracking];
  [self animateBottomSheetHeight:restHeight
                      completion:^{
                        [self.panTracker startTracking];
                      }];
}

- (void)animateBottomSheetHeight:(CGFloat)height
                      completion:(ProceduralBlock)completion {
  [UIView animateWithDuration:0.2
      delay:0
      usingSpringWithDamping:0.9
      initialSpringVelocity:5
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        self.bottomSheetHeight = height;
        [self.view layoutIfNeeded];
        NSArray<UIView*>* attachedViews =
            [self.sheetDelegate lensOverlayBottomSheetAttachedViews:self];
        for (UIView* attachedView in attachedViews) {
          [attachedView layoutIfNeeded];
        }
      }
      completion:^(BOOL) {
        completion();
      }];
}

- (BOOL)isInLargestDetent {
  LensOverlayBottomSheetDetent* currentDetent =
      [self bottomSheetRestDetentWithVelocity:0];
  for (LensOverlayBottomSheetDetent* detent in self.detents) {
    if (detent.value > currentDetent.value) {
      return NO;
    }
  }

  return YES;
}

// Returns the interval of height the bottom sheet can move freely without being
// subject to the visual deceleration.
- (std::pair<CGFloat, CGFloat>)unrestrictedDetentMovementInterval {
  CGFloat maximumDetentValue = 0;
  CGFloat minimumDetentValue = self.detents.firstObject.value;

  for (LensOverlayBottomSheetDetent* detent in self.detents) {
    if (detent.value < minimumDetentValue) {
      minimumDetentValue = detent.value;
    }
    if (detent.value > maximumDetentValue) {
      maximumDetentValue = detent.value;
    }
  }

  // The minimum end is only taken into account if the bottom sheet's closing
  // sequence is prevented.
  if ([self bottomSheetShouldClose]) {
    return std::pair<CGFloat, CGFloat>(0, maximumDetentValue);
  } else {
    return std::pair<CGFloat, CGFloat>(minimumDetentValue, maximumDetentValue);
  }
}

- (CGFloat)bottomSheetRestHeight {
  CGFloat minimumDetentValue = self.detents.firstObject.value;
  for (LensOverlayBottomSheetDetent* detent in self.detents) {
    if (minimumDetentValue > detent.value) {
      minimumDetentValue = detent.value;
    }
    if ([detent.identifier isEqualToString:self.selectedDetentIdentifier]) {
      return [self heightWithOptionalKeyboardIncrease:detent.value];
    }
  }

  // If the selectedDetentIdentifier is `nil`, use the minimum value at the
  // bottom sheet rest height.
  if (!self.selectedDetentIdentifier) {
    return minimumDetentValue;
  }

  NOTREACHED() << "The non nil selectedDetentIdentifier should be one of the "
                  " available detents in the bottom sheet";
}

- (LensOverlayBottomSheetDetent*)bottomSheetRestDetentWithVelocity:
    (CGFloat)velocity {
  // Estimates the bottom sheet final position, assuming a continuous
  // decelerating movement for a brief time. This estimation is not visually
  // displayed, but  used to
  CGFloat estimatedFinalRestingHeight =
      self.bottomSheetHeight + kTimeToSteadyStateAfterRelease * velocity;

  BOOL shouldClose = [self bottomSheetShouldClose];
  BOOL underCloseThreshold =
      estimatedFinalRestingHeight < kCloseBottomSheetThreshold;
  if (shouldClose && underCloseThreshold) {
    return nil;
  }

  // Pickes the closest detent to the estimated final resting height.
  LensOverlayBottomSheetDetent* closestDetent = nil;
  for (LensOverlayBottomSheetDetent* candidateClosest in self.detents) {
    if (!closestDetent) {
      closestDetent = candidateClosest;
      continue;
    }

    CGFloat currentClosestDistance =
        abs(closestDetent.value - estimatedFinalRestingHeight);
    CGFloat candidateClosestDistance =
        abs(candidateClosest.value - estimatedFinalRestingHeight);
    if (candidateClosestDistance < currentClosestDistance) {
      closestDetent = candidateClosest;
    }
  }

  return closestDetent;
}

- (CGFloat)bottomSheetHeight {
  // The visible bottom sheet height is the size of the sheet minus the part
  // that slid under the bottom.
  return _bottomSheetHeightConstraint.constant -
         _bottomSheetBottomConstraint.constant;
}

- (void)setBottomSheetHeight:(CGFloat)height {
  // The height is capped once it reaches `kBottomSheetMinimumDisplayHeight`,
  // and the bottom sheet starts sliding down instead of resizing.
  if (height < kBottomSheetMinimumDisplayHeight) {
    _bottomSheetHeightConstraint.constant = kBottomSheetMinimumDisplayHeight;
    _bottomSheetBottomConstraint.constant =
        kBottomSheetMinimumDisplayHeight - height;
  } else {
    _bottomSheetHeightConstraint.constant = height;
    _bottomSheetBottomConstraint.constant = 0;
  }
}

- (BOOL)bottomSheetShouldClose {
  if (self.detentsDelegate) {
    return [self.detentsDelegate lensOverlayBottomSheetShouldDismiss:self];
  }

  return YES;
}

- (void)constraintVisibleAreaBottomSheetTo:(NSLayoutAnchor*)anchor {
  if (_visibleAreaBottomConstraint) {
    [self.view removeConstraint:_visibleAreaBottomConstraint];
  }
  _visibleAreaBottomConstraint =
      [_visibleAreaLayoutGuide.bottomAnchor constraintEqualToAnchor:anchor];
  _visibleAreaBottomConstraint.active = YES;
}

// The maximum height the bottom sheet can move within the view.
- (CGFloat)bottomSheetContainerHeight {
  return self.view.frame.size.height;
}

#pragma mark - LensOverlayPanTrackerDelegate

// Called when the tracker ended recognizing a pan gesture.
- (void)lensOverlayPanTracker:(LensOverlayPanTracker*)panTracker
    didEndPanGestureWithVelocity:(CGPoint)velocity {
  [self.view endEditing:YES];
  for (UIScrollView* (^scrollViewProvider)() in _disabledScrollViews) {
    UIScrollView* scrollView = scrollViewProvider();
    scrollView.scrollEnabled = YES;
  }
  _disabledScrollViews = [NSMutableArray new];

  [_panTracker stopTracking];

  CGFloat sheetVelocity = -velocity.y;

  LensOverlayBottomSheetDetent* restDetent =
      [self bottomSheetRestDetentWithVelocity:sheetVelocity];
  CGFloat futureHeight =
      [self heightWithOptionalKeyboardIncrease:restDetent.value];
  _selectedDetentIdentifier = restDetent.identifier;

  __weak __typeof(self) weakSelf = self;
  BOOL exitingBottomSheet = futureHeight == 0;
  if (!exitingBottomSheet) {
    [self
        animateBottomSheetHeight:futureHeight
                      completion:^{
                        [weakSelf.detentsDelegate
                            lensOverlayBottomSheetDidChangeSelectedDetentIdentifier:
                                weakSelf];
                        [weakSelf.panTracker startTracking];
                      }];

    return;
  }

  [self dismissAnimated:YES
          gestureDriven:YES
             completion:^{
               [weakSelf.detentsDelegate
                   lensOverlayBottomSheetDidChangeSelectedDetentIdentifier:
                       weakSelf];
             }];
}

- (void)lensOverlayPanTracker:(LensOverlayPanTracker*)panTracker
        didPanWithTranslation:(CGPoint)translation
                     velocity:(CGPoint)velocity {
  std::pair<CGFloat, CGFloat> unrestrictedMovementInterval =
      [self unrestrictedDetentMovementInterval];
  CGFloat minimumDetentValue = unrestrictedMovementInterval.first;
  CGFloat maximumDetentValue = unrestrictedMovementInterval.second;

  CGFloat verticalMovement = -translation.y;
  CGFloat newHeight = MAX([self bottomSheetRestHeight] + verticalMovement, 0);

  // A slight, continuous deceleration is applied to the bottom sheet's movement
  // for a small distance (a couple of pixels) past the top and bottom of its
  // allowed interval. This 'braking' mechanism provides smoother, more
  // continuous feedback to the user, making the sheet's endpoints feel less
  // abrupt.
  if (newHeight > maximumDetentValue) {
    CGFloat diff = newHeight - maximumDetentValue;
    CGFloat constrainedHeightMovement = kRubberBandCoefficient * log(diff);
    newHeight = maximumDetentValue + constrainedHeightMovement;
  } else if (newHeight < minimumDetentValue) {
    CGFloat diff = minimumDetentValue - newHeight;
    CGFloat constrainedHeightMovement = kRubberBandCoefficient * log(diff);
    newHeight = MAX(minimumDetentValue - constrainedHeightMovement, 0);
  }

  [self setBottomSheetHeight:newHeight];
}

- (BOOL)lensOverlayPanTracker:(LensOverlayPanTracker*)panTracker
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)gestureRecognizer {
  // Only handle gestures in the bottom sheet itself.
  BOOL inBottomSheet =
      [gestureRecognizer.view isDescendantOfView:_bottomSheet.view];
  if (!inBottomSheet) {
    return YES;
  }

  UIView* gestureView = gestureRecognizer.view;
  BOOL isPanRecognizer =
      [gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]];
  BOOL isScrollable = [gestureView isKindOfClass:[UIScrollView class]];
  BOOL controlsScrolling = isScrollable && isPanRecognizer;

  BOOL isWKGesture = [self viewDescendantOfWKWebView:gestureView];

  // WebKit internal gestures must not be recognized at the same time as the
  // bottom sheet tracker, because moving the web view's container would
  // interfere with their tracking.
  BOOL internalNonScrollWKGesture = !isScrollable && isWKGesture;
  if (internalNonScrollWKGesture) {
    return NO;
  }

  // Any other gesture that does not handle scrolling should not block the
  // panning of the bottom sheet.
  if (!controlsScrolling) {
    return YES;
  }

  // Although a `WKWebView` can contain multiple internal scroll views, only the
  // main scroll view, which controls the viewport scrolling, should affect the
  // sheet's panning.
  BOOL isMainWKScrollView =
      [gestureView.superview isKindOfClass:[WKWebView class]];

  // Refrain from moving the bottom sheet when a gesture is handling sub-scrolls
  // within a `WKWebView` (e.g.; a draggable HTML element) instead of the
  // primary viewport scroll.
  BOOL handlesInnerWKWebViewScrolls =
      controlsScrolling && isWKGesture && !isMainWKScrollView;
  if (handlesInnerWKWebViewScrolls) {
    return NO;
  }

  UIPanGestureRecognizer* panRecognizer =
      (UIPanGestureRecognizer*)gestureRecognizer;
  __weak UIScrollView* scrollView = (UIScrollView*)gestureView;

  CGPoint translation = [panRecognizer translationInView:scrollView];

  // Horizontal scroll should not drag the bottom sheet.
  BOOL verticalScroll = abs(translation.y) > 3 * abs(translation.x);
  if (!verticalScroll) {
    return NO;
  }

  CGFloat verticalVelocity = [panRecognizer velocityInView:scrollView].y;
  BOOL draggingDown = verticalVelocity > 0;
  BOOL isAtTop = scrollView.contentOffset.y <= 0;
  BOOL isInLargestDetent = [self isInLargestDetent];

  // Overscrolling on top when in the largest detent should defer the gesture to
  // the pan tracker instead of beign handled by the scroll view.
  BOOL sheetMovementForLargestDetent =
      isInLargestDetent && isAtTop && draggingDown;
  // For all other detents, if the scroll view is at the top, the gesture should
  // be translation to the bottom sheet in both directions. This implies that:
  //  - Scrolling the content should only begging in the largest detent.
  //  - Scrolling in the lower detents should be allowed, but it should be a
  // continuation of a previous scroll, rather than starting from scratch.
  BOOL sheetMovementForSmallerDetents = !isInLargestDetent && isAtTop;

  if (sheetMovementForLargestDetent || sheetMovementForSmallerDetents) {
    // Enable dragging of the bottom sheet while disabling scrolling in all
    // referenced scroll views. Re-enable scrolling once the bottom sheet
    // movement finishes.
    if (scrollView.scrollEnabled) {
      [_disabledScrollViews addObject:^{
        return scrollView;
      }];
      scrollView.scrollEnabled = NO;
    }

    return YES;
  }

  // Prevent dragging the bottom sheet and allow only the pan interactions
  // withing the scroll view.
  return NO;
}

#pragma mark - ViewTouchDelegate

- (BOOL)respondToPointInside:(CGPoint)point
                   withEvent:(UIEvent*)event
                     forView:(UIView*)view {
  // Forward touches that fall outside the bottom sheet frame to the view
  // behind.
  return CGRectContainsPoint(_bottomSheet.view.frame, point);
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillShow:(NSNotification*)notification {
  _keyboardShown = YES;
  if (!self.panTracker.isPanning) {
    [self setSelectedDetentIdentifier:self.selectedDetentIdentifier
                             animated:YES];
  }
}

- (void)keyboardWillHide:(NSNotification*)notification {
  _keyboardShown = NO;
  if (!self.panTracker.isPanning) {
    [self setSelectedDetentIdentifier:self.selectedDetentIdentifier
                             animated:YES];
  }
}

// Optionally adds a small increase in the height if the keyboard is shown and
// the largest detent is selected.
- (CGFloat)heightWithOptionalKeyboardIncrease:(CGFloat)height {
  CGFloat result = height;
  if ([self isInLargestDetent] && _keyboardShown) {
    result = height + kKeyboardSheetHeightIncrease;
  }

  return std::clamp<CGFloat>(result, 0, [self bottomSheetContainerHeight]);
}

- (BOOL)viewDescendantOfWKWebView:(UIView*)targetView {
  UIView* currentView = targetView;
  while (currentView != nil) {
    if ([currentView isKindOfClass:[WKWebView class]]) {
      return YES;
    }
    currentView = currentView.superview;
  }
  return NO;
}

@end
