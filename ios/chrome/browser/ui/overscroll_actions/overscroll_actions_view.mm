// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/ios/block_types.h"
#include "base/logging.h"
#include "base/numerics/math_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Actions images.
NSString* const kNewTabActionImage = @"ptr_new_tab";
NSString* const kNewTabActionActiveImage = @"ptr_new_tab_active";
NSString* const kReloadActionImage = @"ptr_reload";
NSString* const kReloadActionActiveImage = @"ptr_reload_active";
NSString* const kCloseActionImage = @"ptr_close";
NSString* const kCloseActionActiveImage = @"ptr_close_active";

// Represents a simple min/max range.
typedef struct {
  CGFloat min;
  CGFloat max;
} FloatRange;

// The threshold at which the refresh actions will start to be visible.
const CGFloat kRefreshThreshold = 48.0;
// The threshold at which the actions are fully visible and can be selected.
const CGFloat kFullThreshold = 56.0;
// The size in point of the edges of the action selection circle layer.
const CGFloat kSelectionEdge = 64.0;
// Initial start position in X of the left and right actions from the center.
// Left actions will start at center.x - kActionsStartPositionMarginFromCenter
// Right actions will start at center.x + kActionsStartPositionMarginFromCenter
const CGFloat kActionsStartPositionMarginFromCenter = 80.0;
// Ranges mapping the width of the screen to the margin of the left and right
// actions images from the frame center.
const FloatRange kActionsPositionMarginsFrom = {320.0, 736.0};
const FloatRange kActionsPositionMarginsTo = {100.0, 200.0};
// Horizontal threshold before visual feedback starts. Threshold applied on
// values in between [-1,1], where -1 corresponds to the leftmost action, and 1
// corresponds to the rightmost action.
const CGFloat kDistanceWhereMovementIsIgnored = 0.1;
// Start scale of the action selection circle when no actions are displayed.
const CGFloat kSelectionInitialDownScale = 0.1;
// Start scale of the action selection circle when actions are displayed but
// no action is selected.
const CGFloat kSelectionDownScale = 0.1875;
// The duration of the animations played when the actions are ready to
// be triggered.
const CGFloat kDisplayActionAnimationDuration = 0.5;
// The duration for the fade animation for an individual action label.  If one
// label is being faded out and another is faded in, the total animation
// duration is twice this value.
const CGFloat kActionLabelFadeDuration = 0.1;
// The final scale of the animation played when an action is triggered.
const CGFloat kDisplayActionAnimationScale = 20;
// This controls how much the selection needs to be moved from the action center
// in order to be snapped to the next action.
// This value must stay in the interval [0,1].
const CGFloat kSelectionSnappingOffsetFromCenter = 0.15;
// Duration of the snapping animation moving the selection circle to the
// selected action.
const CGFloat kSelectionSnappingAnimationDuration = 0.2;
// Controls how much the bezier shape's front and back are deformed.
CGFloat KBezierPathFrontDeformation = 5.0;
CGFloat KBezierPathBackDeformation = 2.5;
// Controls the amount of points the bezier path is made of.
int kBezierPathPointCount = 40;
// Value in point to which the action icon frame will be expanded to detect user
// direct touches.
const CGFloat kDirectTouchFrameExpansion = 20;
// The vertical padding between the bottom of the action image view and its
// corresponding label.
const CGFloat kActionLabelVerticalPadding = 25.0;
// The minimum distance between the action labels and the side of the screen.
const CGFloat kActionLabelSidePadding = 15.0;

// This function maps a value from a range to another.
CGFloat MapValueToRange(FloatRange from, FloatRange to, CGFloat value) {
  DCHECK(from.min < from.max);
  if (value <= from.min)
    return to.min;
  if (value >= from.max)
    return to.max;
  const CGFloat fromDst = from.max - from.min;
  const CGFloat toDst = to.max - to.min;
  return to.min + ((value - from.min) / fromDst) * toDst;
}

// Used to set the X position of a CALayer.
void SetLayerPositionX(CALayer* layer, CGFloat value) {
  CGPoint position = layer.position;
  position.x = value;
  layer.position = position;
}

// Describes the internal state of the OverscrollActionsView.
enum class OverscrollViewState {
  NONE,     // Initial state.
  PREPARE,  // The actions are starting to be displayed.
  READY     // Actions are fully displayed.
};
}  // namespace

// Minimum delay to perform the transition to the ready state.
const CFTimeInterval kMinimumPullDurationToTransitionToReadyInSeconds = 0.25;
// The brightness of the actions view background color for non incognito mode.
const CGFloat kActionViewBackgroundColorBrightnessNonIncognito = 242.0 / 256.0;
// The brightness of the actions view background color for incognito mode.
const CGFloat kActionViewBackgroundColorBrightnessIncognito = 80.0 / 256.0;

@interface OverscrollActionsView ()<UIGestureRecognizerDelegate> {
  // True when the first layout has been done.
  BOOL _initialLayoutDone;
  // True when an action trigger animation is currently playing.
  BOOL _animatingActionTrigger;
  // Whether the selection circle is deformed.
  BOOL _deformationBehaviorEnabled;
  // Whether the view already made the transition to the READY state at least
  // once.
  BOOL _didTransitionToReadyState;
  // True if the view is directly touched.
  BOOL _viewTouched;
  // An additionnal offset added to the horizontalOffset value in order to take
  // into account snapping.
  CGFloat _snappingOffset;
  // The offset of the currently snapped action.
  CGFloat _snappedActionOffset;
  // The value of the horizontalOffset when a snap animation has been triggered.
  CGFloat _horizontalOffsetOnAnimationStart;
  // The last vertical offset.
  CGFloat _lastVerticalOffset;
  // Last recorded pull start absolute time.
  // Unit is in seconds.
  CFTimeInterval _pullStartTimeInSeconds;
  // Tap gesture recognizer that allow the user to tap on an action to activate
  // it.
  UITapGestureRecognizer* _tapGesture;
  // Array of layers that will be centered vertically.
  // The array is built the first time the method -layersToCenterVertically is
  // called.
  NSArray* _layersToCenterVertically;
}

// Redefined to readwrite.
@property(nonatomic, assign, readwrite) OverscrollAction selectedAction;

// Actions image views.
@property(nonatomic, strong) UIImageView* addTabActionImageView;
@property(nonatomic, strong) UIImageView* reloadActionImageView;
@property(nonatomic, strong) UIImageView* closeTabActionImageView;

@property(nonatomic, strong) CALayer* highlightMaskLayer;

@property(nonatomic, strong) UIImageView* addTabActionImageViewHighlighted;
@property(nonatomic, strong) UIImageView* reloadActionImageViewHighlighted;
@property(nonatomic, strong) UIImageView* closeTabActionImageViewHighlighted;

// Action labels.
@property(nonatomic, strong) UILabel* addTabLabel;
@property(nonatomic, strong) UILabel* reloadLabel;
@property(nonatomic, strong) UILabel* closeTabLabel;

// The layer displaying the selection circle.
@property(nonatomic, strong) CAShapeLayer* selectionCircleLayer;
// Mask layer used to display highlighted states when the selection circle is
// above them.
@property(nonatomic, strong) CAShapeLayer* selectionCircleMaskLayer;

// The current vertical offset.
@property(nonatomic, assign) CGFloat verticalOffset;
// The current horizontal offset.
@property(nonatomic, assign) CGFloat horizontalOffset;
// The internal state of the OverscrollActionsView.
@property(nonatomic, assign) OverscrollViewState overscrollState;
// Redefined to readwrite.
@property(nonatomic, strong, readwrite) UIView* backgroundView;
// Snapshot view added on top of the background image view.
@property(nonatomic, strong, readwrite) UIView* snapshotView;
// The parent layer on the selection circle used for cropping purpose.
@property(nonatomic, strong, readwrite) CALayer* selectionCircleCroppingLayer;
// Computed property for whether the current state is incognito or not.
@property(nonatomic, assign, readonly) BOOL incognito;

// An absolute horizontal offset that also takes into account snapping.
- (CGFloat)absoluteHorizontalOffset;
// Computes the margin of the actions image views using the screen's width.
- (CGFloat)actionsPositionMarginFromCenter;
// Performs the layout of the actions image views.
- (void)layoutActions;
// Performs the layout of the action labels.
- (void)layoutActionLabels;
// Absorbs the horizontal movement around the actions in intervals defined with
// kDistanceWhereMovementIsIgnored.
- (CGFloat)absorbsHorizontalMovementAroundActions:(CGFloat)x;
// Computes the position of the selection circle layer based on the horizontal
// offset.
- (CGPoint)selectionCirclePosition;
// Performs layout of the selection circle layer.
- (void)layoutSelectionCircle;
// Updates the selected action depending on the current internal state and
// and the horizontal offset.
- (void)updateSelectedAction;
// Called when the selected action changes in order to perform animations that
// depend on the currently selected action.
- (void)onSelectedActionChangedFromAction:(OverscrollAction)previousAction;
// Layout method used to center subviews vertically.
- (void)centerSubviewsVertically;
// Updates the current internal state of the OverscrollActionsView depending
// on vertical offset.
- (void)updateState;
// Called when the state changes in order to perform state dependent animations.
- (void)onStateChange;
// Resets values related to selection state.
- (void)resetSelection;
// Returns a newly allocated and configured selection circle shape.
- (CAShapeLayer*)newSelectionCircleLayer;
// Returns an autoreleased circular bezier path horizontally deformed according
// to |dx|.
- (UIBezierPath*)circlePath:(CGFloat)dx;
// Returns the action at the given location in the view.
- (OverscrollAction)actionAtLocation:(CGPoint)location;
// Update the selection circle frame to select the given action.
- (void)updateSelectionForTouchedAction:(OverscrollAction)action;
// Clear the direct touch interaction after a small delay to prevent graphic
// glitch with pan gesture selection deformation animations.
- (void)clearDirectTouchInteraction;
// Returns the tooltip label for |action|.
- (UILabel*)labelForAction:(OverscrollAction)action;
// Fades out |previousLabel| and fades in |actionLabel|.
- (void)fadeInActionLabel:(UILabel*)actionLabel
      previousActionLabel:(UILabel*)previousLabel;
@end

@implementation OverscrollActionsView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _deformationBehaviorEnabled = YES;
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.clipsToBounds = YES;
    _selectionCircleLayer = [self newSelectionCircleLayer];
    _selectionCircleMaskLayer = [self newSelectionCircleLayer];
    _selectionCircleMaskLayer.contentsGravity = kCAGravityCenter;
    _selectionCircleCroppingLayer = [[CALayer alloc] init];
    _selectionCircleCroppingLayer.frame = self.bounds;
    [_selectionCircleCroppingLayer setMasksToBounds:YES];

    [self.layer addSublayer:_selectionCircleCroppingLayer];
    [_selectionCircleCroppingLayer addSublayer:_selectionCircleLayer];

    _addTabActionImageView = [[UIImageView alloc] init];
    _addTabActionImageView.image = [[UIImage imageNamed:kNewTabActionImage]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _addTabActionImageView.tintColor = [UIColor colorNamed:kToolbarButtonColor];
    [_addTabActionImageView sizeToFit];
    [self addSubview:_addTabActionImageView];
    _reloadActionImageView = [[UIImageView alloc] init];
    _reloadActionImageView.image = [[UIImage imageNamed:kReloadActionImage]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _reloadActionImageView.tintColor = [UIColor colorNamed:kToolbarButtonColor];
    [_reloadActionImageView sizeToFit];
    if (UseRTLLayout())
      [_reloadActionImageView setTransform:CGAffineTransformMakeScale(-1, 1)];
    [self addSubview:_reloadActionImageView];
    _closeTabActionImageView = [[UIImageView alloc] init];
    _closeTabActionImageView.image = [[UIImage imageNamed:kCloseActionImage]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _closeTabActionImageView.tintColor =
        [UIColor colorNamed:kToolbarButtonColor];
    [_closeTabActionImageView sizeToFit];
    [self addSubview:_closeTabActionImageView];

    _highlightMaskLayer = [[CALayer alloc] init];
    _highlightMaskLayer.frame = self.bounds;
    _highlightMaskLayer.contentsGravity = kCAGravityCenter;
    // Disable the entire highlight mask.
    // TODO(crbug.com/986804): Remove the highlight mask after dark mode
    // launches and this design is permanent.
    _selectionCircleMaskLayer.fillColor = UIColor.clearColor.CGColor;
    [_highlightMaskLayer setMask:_selectionCircleMaskLayer];
    [self.layer addSublayer:_highlightMaskLayer];

    _addTabActionImageViewHighlighted = [[UIImageView alloc] init];
    _reloadActionImageViewHighlighted = [[UIImageView alloc] init];
    if (UseRTLLayout()) {
      [_reloadActionImageViewHighlighted
          setTransform:CGAffineTransformMakeScale(-1, 1)];
    }
    _closeTabActionImageViewHighlighted = [[UIImageView alloc] init];
    [_highlightMaskLayer addSublayer:_addTabActionImageViewHighlighted.layer];
    [_highlightMaskLayer addSublayer:_reloadActionImageViewHighlighted.layer];
    [_highlightMaskLayer addSublayer:_closeTabActionImageViewHighlighted.layer];

    _addTabLabel = [[UILabel alloc] init];
    _addTabLabel.numberOfLines = 0;
    _addTabLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _addTabLabel.textAlignment = NSTextAlignmentLeft;
    _addTabLabel.alpha = 0.0;
    _addTabLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _addTabLabel.adjustsFontForContentSizeCategory = NO;
    _addTabLabel.textColor = [UIColor colorNamed:kToolbarButtonColor];
    _addTabLabel.text =
        l10n_util::GetNSString(IDS_IOS_OVERSCROLL_NEW_TAB_LABEL);
    [self addSubview:_addTabLabel];
    _reloadLabel = [[UILabel alloc] init];
    _reloadLabel.numberOfLines = 0;
    _reloadLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _reloadLabel.textAlignment = NSTextAlignmentCenter;
    _reloadLabel.alpha = 0.0;
    _reloadLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _reloadLabel.adjustsFontForContentSizeCategory = NO;
    _reloadLabel.textColor = [UIColor colorNamed:kToolbarButtonColor];
    _reloadLabel.text = l10n_util::GetNSString(IDS_IOS_OVERSCROLL_RELOAD_LABEL);
    [self addSubview:_reloadLabel];
    _closeTabLabel = [[UILabel alloc] init];
    _closeTabLabel.numberOfLines = 0;
    _closeTabLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _closeTabLabel.textAlignment = NSTextAlignmentRight;
    _closeTabLabel.alpha = 0.0;
    _closeTabLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _closeTabLabel.adjustsFontForContentSizeCategory = NO;
    _closeTabLabel.textColor = [UIColor colorNamed:kToolbarButtonColor];
    _closeTabLabel.text =
        l10n_util::GetNSString(IDS_IOS_OVERSCROLL_CLOSE_TAB_LABEL);
    [self addSubview:_closeTabLabel];

    _backgroundView = [[UIView alloc] initWithFrame:CGRectZero];
    [self addSubview:_backgroundView];

    if (UseRTLLayout()) {
      // Handle RTL using transforms since this class is CALayer-based.
      [self setTransform:CGAffineTransformMakeScale(-1, 1)];
      // Reverse labels again because they are subview of |self|, otherwise they
      // will be rendered backwards.
      [_addTabLabel setTransform:CGAffineTransformMakeScale(-1, 1)];
      [_reloadLabel setTransform:CGAffineTransformMakeScale(-1, 1)];
      [_closeTabLabel setTransform:CGAffineTransformMakeScale(-1, 1)];
    }

    _tapGesture =
        [[UITapGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(tapGesture:)];
    [_tapGesture setDelegate:self];
    [self addGestureRecognizer:_tapGesture];
  }
  return self;
}

- (void)dealloc {
  [self.snapshotView removeFromSuperview];
  ;
}

- (BOOL)selectionCroppingEnabled {
  return [_selectionCircleCroppingLayer masksToBounds];
}

- (void)setSelectionCroppingEnabled:(BOOL)enableSelectionCropping {
  [_selectionCircleCroppingLayer setMasksToBounds:enableSelectionCropping];
}

- (void)addSnapshotView:(UIView*)view {
  if (UseRTLLayout()) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [view setTransform:CGAffineTransformMakeScale(-1, 1)];
    [CATransaction commit];
  }
  [self.snapshotView removeFromSuperview];
  self.snapshotView = view;
  [self.backgroundView addSubview:self.snapshotView];
}

- (void)pullStarted {
  _didTransitionToReadyState = NO;
  _pullStartTimeInSeconds = CACurrentMediaTime();
  // Ensure we will update the state after time threshold even without offset
  // change.
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    (kMinimumPullDurationToTransitionToReadyInSeconds + 0.01) *
                        NSEC_PER_SEC),
      dispatch_get_main_queue(), ^{
        [self updateState];
      });
}

- (void)updateWithVerticalOffset:(CGFloat)offset {
  _lastVerticalOffset = self.verticalOffset;
  self.verticalOffset = offset;
  [self updateState];
}

- (void)updateWithHorizontalOffset:(CGFloat)offset {
  if (_animatingActionTrigger || _viewTouched)
    return;
  self.horizontalOffset = offset;
  // Absorb out of range offset values so that the user doesn't need to
  // compensate in order to move the cursor in the other direction.
  if ([self absoluteHorizontalOffset] < -1)
    _snappingOffset = -self.horizontalOffset - 1;
  if ([self absoluteHorizontalOffset] > 1)
    _snappingOffset = 1 - self.horizontalOffset;
  [self setNeedsLayout];
}

- (void)displayActionAnimation {
  _animatingActionTrigger = YES;
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    _animatingActionTrigger = NO;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    // See comment below for why we manually set opacity to 0 and remove
    // the animation.
    self.selectionCircleLayer.opacity = 0;
    [self.selectionCircleLayer removeAnimationForKey:@"opacity"];
    [self onStateChange];
    [CATransaction commit];
  }];

  CABasicAnimation* scaleAnimation =
      [CABasicAnimation animationWithKeyPath:@"transform"];
  scaleAnimation.fromValue =
      [NSValue valueWithCATransform3D:CATransform3DIdentity];
  scaleAnimation.toValue =
      [NSValue valueWithCATransform3D:CATransform3DMakeScale(
                                          kDisplayActionAnimationScale,
                                          kDisplayActionAnimationScale, 1)];
  scaleAnimation.duration = kDisplayActionAnimationDuration;
  [self.selectionCircleLayer addAnimation:scaleAnimation forKey:@"transform"];

  CABasicAnimation* opacityAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  opacityAnimation.fromValue = @(1);
  opacityAnimation.toValue = @(0);
  opacityAnimation.duration = kDisplayActionAnimationDuration;
  // A fillMode forward and manual removal of the animation is needed because
  // the completion handler can be called one frame earlier for the first
  // animation (transform) causing the opacity animation to be removed and show
  // an opacity of 1 for one or two frames.
  opacityAnimation.fillMode = kCAFillModeForwards;
  opacityAnimation.removedOnCompletion = NO;
  [self.selectionCircleLayer addAnimation:opacityAnimation forKey:@"opacity"];

  [CATransaction commit];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  if (self.snapshotView)
    self.backgroundView.frame = self.snapshotView.bounds;
  _selectionCircleCroppingLayer.frame = self.bounds;
  _highlightMaskLayer.frame = self.bounds;

  [CATransaction commit];

  const BOOL disableActionsOnInitialLayout =
      !CGRectEqualToRect(CGRectZero, self.frame) && !_initialLayoutDone;
  if (disableActionsOnInitialLayout) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _initialLayoutDone = YES;
  }
  [self centerSubviewsVertically];
  [self layoutActions];
  [self layoutActionLabels];
  if (_deformationBehaviorEnabled)
    [self layoutSelectionCircleWithDeformation];
  else
    [self layoutSelectionCircle];
  [self updateSelectedAction];
  if (disableActionsOnInitialLayout)
    [CATransaction commit];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLayerColors];
}

#pragma mark - Private

- (CGFloat)absoluteHorizontalOffset {
  return self.horizontalOffset + _snappingOffset;
}

- (CGFloat)actionsPositionMarginFromCenter {
  return MapValueToRange(kActionsPositionMarginsFrom, kActionsPositionMarginsTo,
                         self.bounds.size.width);
}

- (void)layoutActions {
  const CGFloat width = self.bounds.size.width;
  const CGFloat centerX = width / 2.0;
  const CGFloat actionsPositionMargin = [self actionsPositionMarginFromCenter];

  [UIView beginAnimations:@"position" context:NULL];
  [UIView setAnimationDuration:0.1];
  SetLayerPositionX(self.reloadActionImageView.layer, centerX);
  SetLayerPositionX(self.reloadActionImageViewHighlighted.layer, centerX);

  const CGFloat addTabPositionX =
      MapValueToRange({kRefreshThreshold, kFullThreshold},
                      {centerX - kActionsStartPositionMarginFromCenter,
                       centerX - actionsPositionMargin},
                      self.verticalOffset);
  SetLayerPositionX(self.addTabActionImageView.layer, addTabPositionX);
  SetLayerPositionX(self.addTabActionImageViewHighlighted.layer,
                    addTabPositionX);

  const CGFloat closeTabPositionX =
      MapValueToRange({kRefreshThreshold, kFullThreshold},
                      {centerX + kActionsStartPositionMarginFromCenter,
                       centerX + actionsPositionMargin},
                      self.verticalOffset);
  SetLayerPositionX(self.closeTabActionImageView.layer, closeTabPositionX);
  SetLayerPositionX(self.closeTabActionImageViewHighlighted.layer,
                    closeTabPositionX);

  [UIView commitAnimations];

  [UIView beginAnimations:@"opacity" context:NULL];
  [UIView setAnimationDuration:0.1];
  self.reloadActionImageView.layer.opacity = MapValueToRange(
      {kFullThreshold / 2.0, kFullThreshold}, {0, 1}, self.verticalOffset);
  self.reloadActionImageViewHighlighted.layer.opacity =
      self.reloadActionImageView.layer.opacity;
  self.addTabActionImageView.layer.opacity = MapValueToRange(
      {kRefreshThreshold, kFullThreshold}, {0, 1}, self.verticalOffset);
  self.addTabActionImageViewHighlighted.layer.opacity =
      self.addTabActionImageView.layer.opacity;
  self.closeTabActionImageView.layer.opacity = MapValueToRange(
      {kRefreshThreshold, kFullThreshold}, {0, 1}, self.verticalOffset);
  self.closeTabActionImageViewHighlighted.layer.opacity =
      self.closeTabActionImageView.layer.opacity;
  [UIView commitAnimations];

  [UIView beginAnimations:@"transform" context:NULL];
  [UIView setAnimationDuration:0.1];
  CATransform3D rotation = CATransform3DMakeRotation(
      MapValueToRange({kFullThreshold / 2.0, kFullThreshold},
                      {-base::kPiFloat / 2, base::kPiFloat / 4},
                      self.verticalOffset),
      0, 0, 1);
  self.reloadActionImageView.layer.transform = rotation;
  self.reloadActionImageViewHighlighted.layer.transform = rotation;
  [UIView commitAnimations];
}

- (void)layoutActionLabels {
  // The text is truncated to be a maximum of half the width of the view.
  CGSize boundingSize = self.bounds.size;
  boundingSize.width /= 2.0;

  // The UILabels in |labels| are laid out according to the location of their
  // corresponding UIImageView in |images|.
  NSArray* labels = @[ self.addTabLabel, self.reloadLabel, self.closeTabLabel ];
  NSArray* images = @[
    self.addTabActionImageView, self.reloadActionImageView,
    self.closeTabActionImageView
  ];

  [labels enumerateObjectsUsingBlock:^(UILabel* label, NSUInteger idx, BOOL*) {
    UIImageView* image = images[idx];
    CGRect frame = CGRectZero;
    frame.size = [label sizeThatFits:boundingSize];
    frame.origin.x = image.center.x - frame.size.width / 2.0;
    frame.origin.x = fmaxf(
        frame.origin.x, CGRectGetMinX(self.bounds) + kActionLabelSidePadding);
    frame.origin.x = fminf(frame.origin.x, CGRectGetMaxX(self.bounds) -
                                               kActionLabelSidePadding -
                                               CGRectGetWidth(frame));
    frame.origin.y = image.center.y + CGRectGetHeight(image.bounds) / 2.0 +
                     kActionLabelVerticalPadding;
    label.frame = frame;
  }];
}

- (CGFloat)absorbsHorizontalMovementAroundActions:(CGFloat)x {
  // The limits of the intervals where x is constant.
  const CGFloat kLeftActionAbsorptionLimit =
      -1 + kDistanceWhereMovementIsIgnored;
  const CGFloat kCenterActionLeftAbsorptionLimit =
      -kDistanceWhereMovementIsIgnored;
  const CGFloat kCenterActionRightAbsorptionLimit =
      kDistanceWhereMovementIsIgnored;
  const CGFloat kRightActionAbsorptionLimit =
      1 - kDistanceWhereMovementIsIgnored;
  if (x < kLeftActionAbsorptionLimit) {
    return -1;
  }
  if (x < kCenterActionLeftAbsorptionLimit) {
    return MapValueToRange(
        {kLeftActionAbsorptionLimit, kCenterActionLeftAbsorptionLimit}, {-1, 0},
        x);
  }
  if (x < kCenterActionRightAbsorptionLimit) {
    return 0;
  }
  if (x < kRightActionAbsorptionLimit) {
    return MapValueToRange(
        {kCenterActionRightAbsorptionLimit, kRightActionAbsorptionLimit},
        {0, 1}, x);
  }
  return 1;
}

- (CGPoint)selectionCirclePosition {
  const CGFloat centerX = self.bounds.size.width / 2.0;
  const CGFloat actionsPositionMargin = [self actionsPositionMarginFromCenter];
  const CGFloat transformedOffset = [self
      absorbsHorizontalMovementAroundActions:[self absoluteHorizontalOffset]];
  return CGPointMake(MapValueToRange({-1, 1}, {centerX - actionsPositionMargin,
                                               centerX + actionsPositionMargin},
                                     transformedOffset),
                     self.bounds.size.height / 2.0);
}

- (void)layoutSelectionCircle {
  if (self.overscrollState == OverscrollViewState::READY) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.selectionCircleLayer.position = [self selectionCirclePosition];
    self.selectionCircleMaskLayer.position = self.selectionCircleLayer.position;
    [CATransaction commit];
  }
}

- (void)layoutSelectionCircleWithDeformation {
  if (self.overscrollState == OverscrollViewState::READY) {
    BOOL animate = NO;
    CGFloat snapDistance =
        [self absoluteHorizontalOffset] - _snappedActionOffset;
    // Cancel out deformation for small movements.
    if (fabs(snapDistance) < kDistanceWhereMovementIsIgnored) {
      snapDistance = 0;
    } else {
      snapDistance -= snapDistance > 0 ? kDistanceWhereMovementIsIgnored
                                       : -kDistanceWhereMovementIsIgnored;
    }

    [self.selectionCircleLayer removeAnimationForKey:@"path"];
    [self.selectionCircleMaskLayer removeAnimationForKey:@"path"];
    self.selectionCircleLayer.path = [self circlePath:snapDistance].CGPath;
    self.selectionCircleMaskLayer.path = self.selectionCircleLayer.path;

    if (fabs(snapDistance) > kSelectionSnappingOffsetFromCenter) {
      animate = YES;
      _snappedActionOffset += (snapDistance < 0 ? -1 : 1);
      _snappingOffset = _snappedActionOffset - self.horizontalOffset;
      _horizontalOffsetOnAnimationStart = self.horizontalOffset;
      const CGFloat finalSnapDistance =
          [self absoluteHorizontalOffset] - _snappedActionOffset;

      UIBezierPath* finalPath = [self circlePath:finalSnapDistance];
      [CATransaction begin];
      [CATransaction setCompletionBlock:^{
        self.selectionCircleLayer.path = finalPath.CGPath;
        [self.selectionCircleLayer removeAnimationForKey:@"path"];
        self.selectionCircleMaskLayer.path = finalPath.CGPath;
        [self.selectionCircleMaskLayer removeAnimationForKey:@"path"];
      }];
      CABasicAnimation* (^pathAnimation)(void) = ^{
        CABasicAnimation* pathAnim =
            [CABasicAnimation animationWithKeyPath:@"path"];
        pathAnim.removedOnCompletion = NO;
        pathAnim.fillMode = kCAFillModeForwards;
        pathAnim.duration = kSelectionSnappingAnimationDuration;
        pathAnim.toValue = (__bridge id)finalPath.CGPath;
        return pathAnim;
      };
      [self.selectionCircleLayer addAnimation:pathAnimation() forKey:@"path"];
      [self.selectionCircleMaskLayer addAnimation:pathAnimation()
                                           forKey:@"path"];
      [CATransaction commit];
    }
    [CATransaction begin];
    if (!animate)
      [CATransaction setDisableActions:YES];
    else
      [CATransaction setAnimationDuration:kSelectionSnappingAnimationDuration];
    self.selectionCircleLayer.position = [self selectionCirclePosition];
    self.selectionCircleMaskLayer.position = self.selectionCircleLayer.position;
    [CATransaction commit];
  }
}

- (void)updateSelectedAction {
  if (self.overscrollState != OverscrollViewState::READY) {
    self.selectedAction = OverscrollAction::NONE;
    return;
  }

  // Update action index by checking that the action image layer is included
  // inside the selection layer.
  const CGPoint selectionPosition = [self selectionCirclePosition];
  if (_deformationBehaviorEnabled) {
    const CGFloat distanceBetweenTwoActions =
        (self.reloadActionImageView.frame.origin.x -
         self.addTabActionImageView.frame.origin.x) /
        2;
    if (fabs(self.addTabActionImageView.center.x - selectionPosition.x) <
        distanceBetweenTwoActions) {
      self.selectedAction = OverscrollAction::NEW_TAB;
    }
    if (fabs(self.reloadActionImageView.center.x - selectionPosition.x) <
        distanceBetweenTwoActions) {
      self.selectedAction = OverscrollAction::REFRESH;
    }
    if (fabs(self.closeTabActionImageView.center.x - selectionPosition.x) <
        distanceBetweenTwoActions) {
      self.selectedAction = OverscrollAction::CLOSE_TAB;
    }
  } else {
    const CGRect selectionRect =
        CGRectMake(selectionPosition.x - kSelectionEdge / 2.0,
                   selectionPosition.y - kSelectionEdge / 2.0, kSelectionEdge,
                   kSelectionEdge);
    const CGRect addTabRect = self.addTabActionImageView.frame;
    const CGRect closeTabRect = self.closeTabActionImageView.frame;
    const CGRect refreshRect = self.reloadActionImageView.frame;

    if (CGRectContainsRect(selectionRect, addTabRect)) {
      self.selectedAction = OverscrollAction::NEW_TAB;
    } else if (CGRectContainsRect(selectionRect, refreshRect)) {
      self.selectedAction = OverscrollAction::REFRESH;
    } else if (CGRectContainsRect(selectionRect, closeTabRect)) {
      self.selectedAction = OverscrollAction::CLOSE_TAB;
    } else {
      self.selectedAction = OverscrollAction::NONE;
    }
  }
}

- (void)setSelectedAction:(OverscrollAction)action {
  if (_selectedAction != action) {
    OverscrollAction previousAction = _selectedAction;
    _selectedAction = action;
    [self onSelectedActionChangedFromAction:previousAction];
  }
}

- (void)onSelectedActionChangedFromAction:(OverscrollAction)previousAction {
  [self fadeInActionLabel:[self labelForAction:self.selectedAction]
      previousActionLabel:[self labelForAction:previousAction]];

  if (self.overscrollState == OverscrollViewState::PREPARE ||
      _animatingActionTrigger)
    return;

  [UIView beginAnimations:@"transform" context:NULL];
  [UIView setAnimationDuration:kSelectionSnappingAnimationDuration];
  if (self.selectedAction == OverscrollAction::NONE) {
    if (!_deformationBehaviorEnabled) {
      // Scale selection down.
      self.selectionCircleLayer.transform =
          CATransform3DMakeScale(kSelectionDownScale, kSelectionDownScale, 1);
      self.selectionCircleMaskLayer.transform =
          self.selectionCircleLayer.transform;
    }
  } else {
    // Scale selection up.
    self.selectionCircleLayer.transform = CATransform3DMakeScale(1, 1, 1);
    self.selectionCircleMaskLayer.transform =
        self.selectionCircleLayer.transform;
  }
  [UIView commitAnimations];

  [self.delegate overscrollActionsView:self
               selectedActionDidChange:self.selectedAction];
}

- (NSArray*)layersToCenterVertically {
  if (!_layersToCenterVertically) {
    _layersToCenterVertically = @[
      _selectionCircleLayer, _selectionCircleMaskLayer,
      _addTabActionImageView.layer, _reloadActionImageView.layer,
      _closeTabActionImageView.layer, _addTabActionImageViewHighlighted.layer,
      _reloadActionImageViewHighlighted.layer,
      _closeTabActionImageViewHighlighted.layer, _backgroundView.layer
    ];
  }
  return _layersToCenterVertically;
}

- (void)centerSubviewsVertically {
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  for (CALayer* layer in self.layersToCenterVertically) {
    CGPoint position = layer.position;
    position.y = self.bounds.size.height / 2;
    layer.position = position;
  }
  [CATransaction commit];
}

- (void)updateState {
  if (self.verticalOffset > 1) {
    const CFTimeInterval elapsedTime =
        CACurrentMediaTime() - _pullStartTimeInSeconds;
    const BOOL isMinimumTimeElapsed =
        elapsedTime >= kMinimumPullDurationToTransitionToReadyInSeconds;
    const BOOL isPullingDownOrAlreadyTriggeredOnce =
        _lastVerticalOffset <= self.verticalOffset ||
        _didTransitionToReadyState;
    const BOOL isVerticalThresholdSatisfied =
        self.verticalOffset >= kFullThreshold;
    if (isPullingDownOrAlreadyTriggeredOnce && isVerticalThresholdSatisfied &&
        isMinimumTimeElapsed) {
      self.overscrollState = OverscrollViewState::READY;
    } else {
      self.overscrollState = OverscrollViewState::PREPARE;
    }
  } else {
    self.overscrollState = OverscrollViewState::NONE;
  }
  [self setNeedsLayout];
}

- (void)setOverscrollState:(OverscrollViewState)state {
  if (_overscrollState != state) {
    _overscrollState = state;
    [self onStateChange];
  }
}

- (void)onStateChange {
  if (_animatingActionTrigger)
    return;

  if (self.overscrollState != OverscrollViewState::NONE) {
    [UIView beginAnimations:@"opacity" context:NULL];
    [UIView setAnimationDuration:kSelectionSnappingAnimationDuration];
    self.selectionCircleLayer.opacity =
        self.overscrollState == OverscrollViewState::READY ? 1.0 : 0.0;
    self.selectionCircleMaskLayer.opacity = self.selectionCircleLayer.opacity;
    [UIView commitAnimations];
    if (self.overscrollState == OverscrollViewState::PREPARE) {
      [UIView beginAnimations:@"transform" context:NULL];
      [UIView setAnimationDuration:kSelectionSnappingAnimationDuration];
      [self resetSelection];
      [UIView commitAnimations];
    } else {
      _didTransitionToReadyState = YES;
    }
  } else {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [self resetSelection];
    [CATransaction commit];
  }
}

- (void)resetSelection {
  _didTransitionToReadyState = NO;
  _snappingOffset = 0;
  _snappedActionOffset = 0;
  _horizontalOffsetOnAnimationStart = 0;
  self.selectionCircleLayer.transform = CATransform3DMakeScale(
      kSelectionInitialDownScale, kSelectionInitialDownScale, 1);
  self.selectionCircleMaskLayer.transform = self.selectionCircleLayer.transform;
  [self updateSelectedAction];
}

- (CAShapeLayer*)newSelectionCircleLayer {
  const CGRect bounds = CGRectMake(0, 0, kSelectionEdge, kSelectionEdge);
  CAShapeLayer* selectionCircleLayer = [[CAShapeLayer alloc] init];
  selectionCircleLayer.bounds = bounds;
  selectionCircleLayer.backgroundColor = UIColor.clearColor.CGColor;
  selectionCircleLayer.opacity = 0;
  selectionCircleLayer.transform = CATransform3DMakeScale(
      kSelectionInitialDownScale, kSelectionInitialDownScale, 1);
  selectionCircleLayer.path =
      [[UIBezierPath bezierPathWithOvalInRect:bounds] CGPath];

  return selectionCircleLayer;
}

- (UIBezierPath*)circlePath:(CGFloat)dx {
  UIBezierPath* path = [UIBezierPath bezierPath];

  CGFloat radius = kSelectionEdge * 0.5;
  CGFloat deformationDirection = dx > 0 ? 1 : -1;
  for (int i = 0; i < kBezierPathPointCount; i++) {
    CGPoint p;
    float angle = i * 2 * base::kPiFloat / kBezierPathPointCount;

    // Circle centered on 0.
    p.x = cos(angle) * radius;
    p.y = sin(angle) * radius;

    // Horizontal deformation. The further the points are from the center, the
    // larger the deformation is.
    if (p.x * deformationDirection > 0) {
      p.x += p.x * dx * KBezierPathFrontDeformation * deformationDirection;
    } else {
      p.x += p.x * dx * KBezierPathBackDeformation * deformationDirection;
    }

    // Translate center of circle.
    p.x += radius;
    p.y += radius;

    if (i == 0) {
      [path moveToPoint:p];
    } else {
      [path addLineToPoint:p];
    }
  }

  [path closePath];
  return path;
}

- (void)setStyle:(OverscrollStyle)style {
  _style = style;
  switch (self.style) {
    case OverscrollStyle::NTP_NON_INCOGNITO:
      self.backgroundColor = ntp_home::kNTPBackgroundColor();
      break;
    case OverscrollStyle::NTP_INCOGNITO:
      self.backgroundColor = [UIColor colorWithWhite:0 alpha:0];
      break;
    case OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO:
      self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
      break;
    case OverscrollStyle::REGULAR_PAGE_INCOGNITO:
      self.backgroundColor = color::DarkModeDynamicColor(
          [UIColor colorNamed:kBackgroundColor], true,
          [UIColor colorNamed:kBackgroundDarkColor]);
      break;
  }

  [self updateLayerColors];
}

// CGColor doesn't support iOS 13 dynamic colors, so those must be resolved
// more often.
- (void)updateLayerColors {
  if (@available(iOS 13, *)) {
    [self.traitCollection performAsCurrentTraitCollection:^{
      _selectionCircleLayer.fillColor =
          [UIColor colorNamed:kTextfieldBackgroundColor].CGColor;
    }];
    return;
  }

  // Fallback for iOS 12.
  if (self.incognito) {
    UIColor* buttonColor = [UIColor colorNamed:kToolbarButtonDarkColor];
    _addTabActionImageView.tintColor = buttonColor;
    _reloadActionImageView.tintColor = buttonColor;
    _closeTabActionImageView.tintColor = buttonColor;
    _addTabLabel.textColor = buttonColor;
    _reloadLabel.textColor = buttonColor;
    _closeTabLabel.textColor = buttonColor;
    _selectionCircleLayer.fillColor =
        [UIColor colorNamed:kTextfieldBackgroundDarkColor].CGColor;
  } else {
    _selectionCircleLayer.fillColor =
        [UIColor colorNamed:kTextfieldBackgroundColor].CGColor;
  }
}

- (OverscrollAction)actionAtLocation:(CGPoint)location {
  OverscrollAction action = OverscrollAction::NONE;
  if (CGRectContainsPoint(
          CGRectInset([_addTabActionImageView frame],
                      -kDirectTouchFrameExpansion, -kDirectTouchFrameExpansion),
          location)) {
    action = OverscrollAction::NEW_TAB;
  } else if (CGRectContainsPoint(CGRectInset([_reloadActionImageView frame],
                                             -kDirectTouchFrameExpansion,
                                             -kDirectTouchFrameExpansion),
                                 location)) {
    action = OverscrollAction::REFRESH;
  } else if (CGRectContainsPoint(CGRectInset([_closeTabActionImageView frame],
                                             -kDirectTouchFrameExpansion,
                                             -kDirectTouchFrameExpansion),
                                 location)) {
    action = OverscrollAction::CLOSE_TAB;
  }
  return action;
}

- (void)updateSelectionForTouchedAction:(OverscrollAction)action {
  switch (action) {
    case OverscrollAction::NEW_TAB:
      [self updateWithHorizontalOffset:-1];
      break;
    case OverscrollAction::REFRESH:
      [self updateWithHorizontalOffset:0];
      break;
    case OverscrollAction::CLOSE_TAB:
      [self updateWithHorizontalOffset:1];
      break;
    case OverscrollAction::NONE:
      return;
      break;
  }
}

// Clear the direct touch interaction after a small delay to prevent graphic
// glitch with pan gesture selection deformation animations.
- (void)clearDirectTouchInteraction {
  if (!_viewTouched)
    return;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        _deformationBehaviorEnabled = YES;
        _viewTouched = NO;
      });
}

- (UILabel*)labelForAction:(OverscrollAction)action {
  switch (action) {
    case OverscrollAction::NEW_TAB:
      return self.addTabLabel;
    case OverscrollAction::REFRESH:
      return self.reloadLabel;
    case OverscrollAction::CLOSE_TAB:
      return self.closeTabLabel;
    case OverscrollAction::NONE:
      return nil;
  }
}

- (void)fadeInActionLabel:(UILabel*)actionLabel
      previousActionLabel:(UILabel*)previousLabel {
  NSUInteger labelCount = (actionLabel ? 1 : 0) + (previousLabel ? 1 : 0);
  if (!labelCount)
    return;

  NSTimeInterval duration = labelCount * kActionLabelFadeDuration;
  NSTimeInterval relativeDuration = 1.0 / labelCount;
  UIViewKeyframeAnimationOptions options =
      UIViewKeyframeAnimationOptionBeginFromCurrentState;
  ProceduralBlock animations = ^{
    CGFloat startTime = 0.0;
    if (previousLabel) {
      [UIView addKeyframeWithRelativeStartTime:startTime
                              relativeDuration:relativeDuration
                                    animations:^{
                                      previousLabel.alpha = 0.0;
                                    }];
      startTime += relativeDuration;
    }
    if (actionLabel) {
      [UIView addKeyframeWithRelativeStartTime:startTime
                              relativeDuration:relativeDuration
                                    animations:^{
                                      actionLabel.alpha = 1.0;
                                    }];
    }
  };
  [UIView animateKeyframesWithDuration:duration
                                 delay:0
                               options:options
                            animations:animations
                            completion:nil];
}

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];
  if (_viewTouched)
    return;

  _deformationBehaviorEnabled = NO;
  _snappingOffset = 0;
  CGPoint tapLocation = [[touches anyObject] locationInView:self];
  [self updateSelectionForTouchedAction:[self actionAtLocation:tapLocation]];
  [self layoutSubviews];
  _viewTouched = YES;
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  [self clearDirectTouchInteraction];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  [self clearDirectTouchInteraction];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

#pragma mark - Tap gesture action

- (void)tapGesture:(UITapGestureRecognizer*)tapRecognizer {
  CGPoint tapLocation = [tapRecognizer locationInView:self];
  OverscrollAction action = [self actionAtLocation:tapLocation];
  if (action != OverscrollAction::NONE) {
    [self updateSelectionForTouchedAction:action];
    [self setSelectedAction:action];
    [self.delegate overscrollActionsViewDidTapTriggerAction:self];
  }
}

@end
