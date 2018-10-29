// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/fancy_ui/bidi_container_view.h"
#include "ios/chrome/browser/ui/page_info/page_info_model.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_reloading.h"
#import "ios/chrome/browser/ui/util/animation_util.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ios::material::TimingFunction;

namespace {

// The width of the view.
const CGFloat kViewWidthRegular = 600.0;
const CGFloat kViewWidthCompact = 288.0;
const CGFloat kViewWidthiPhoneLandscape = 400.0;
// Spacing in between sections.
const CGFloat kVerticalSpacing = 20.0;
// Initial position for the left side of the frame.
const CGFloat kInitialFramePosition = 8.0;
// Alpha for the shield.
const CGFloat kShieldAlpha = 0.5;
// Scroll View inset.
const CGFloat kScrollViewInset = 5.0;
// The size of the footer (rounded corner and shadow) for page info view.
const CGFloat kPageInfoViewFooterSize = 15.0;
// Padding between the window frame and content.
const CGFloat kFramePadding = 24;
// Padding for the initial line of the view.
const CGFloat kInitialLinePadding = 40;
// Padding between the bottom of the content and the window frame.
const CGFloat kFrameBottomPadding = 16;
// Spacing between the optional headline and description text views.
const CGFloat kHeadlineSpacing = 16;
// Spacing between the image and the text.
const CGFloat kImageSpacing = 16;
// Square size of the image.
const CGFloat kImageSize = 24;
// The height of the headline label.
const CGFloat kHeadlineHeight = 19;
// The hex color for the help button text.
const int kPageInfoHelpButtonRGB = 0x3b8cfe;
// The grey scale color for the text within the page info alert.
const CGFloat kPageInfoTextGreyComponent = 0.2;

inline UIColor* PageInfoTextColor() {
  return [UIColor colorWithWhite:kPageInfoTextGreyComponent alpha:1];
}

inline UIColor* PageInfoHelpButtonColor() {
  return UIColorFromRGB(kPageInfoHelpButtonRGB);
}

inline UIFont* PageInfoHeadlineFont() {
  return [[MDCTypography fontLoader] mediumFontOfSize:16];
}

inline CATransform3D PageInfoAnimationScale() {
  return CATransform3DMakeScale(0.03, 0.03, 1);
}

// Offset to make sure image aligns with the header line.
inline CGFloat PageInfoImageVerticalOffset() {
  return ui::AlignValueToUpperPixel((kHeadlineHeight - kImageSize) / 2.0);
}

// The X position of the text fields. Variants for with and without an image.
const CGFloat kTextXPositionNoImage = kFramePadding;
const CGFloat kTextXPosition =
    kTextXPositionNoImage + kImageSize + kImageSpacing;

// The X offset for the help button.
const CGFloat kButtonXOffset = kTextXPosition;

}  // namespace

PageInfoModelBubbleBridge::PageInfoModelBubbleBridge()
    : controller_(nil), weak_ptr_factory_(this) {}

PageInfoModelBubbleBridge::~PageInfoModelBubbleBridge() {}

void PageInfoModelBubbleBridge::OnPageInfoModelChanged() {
  // Check to see if a layout has already been scheduled.
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  // Delay performing layout by a second so that all the animations from
  // InfoBubbleWindow and origin updates from BaseBubbleController finish, so
  // that we don't all race trying to change the frame's origin.
  //
  // Using MessageLoop is superior here to |-performSelector:| because it will
  // not retain its target; if the child outlives its parent, zombies get left
  // behind (http://crbug.com/59619). This will cancel the scheduled task if
  // the controller (and thus this bridge) get destroyed before the message
  // can be delivered.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PageInfoModelBubbleBridge::PerformLayout,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(1000 /* milliseconds */));
}

void PageInfoModelBubbleBridge::PerformLayout() {
  // If the window is animating closed when this is called, the
  // animation could be holding the last reference to |controller_|
  // (and thus |this|).  Pin it until the task is completed.
  NS_VALID_UNTIL_END_OF_SCOPE PageInfoViewController* keep_alive = controller_;
  [controller_ performLayout];
}

@interface PageInfoViewController ()<UIGestureRecognizerDelegate> {
  // Scroll View inside the PageInfoView used to display content that exceeds
  // the available space.
  UIScrollView* scrollView_;
  // Container View added inside the Scroll View. All content is added to this
  // view instead of PopupMenuController.containerView_.
  BidiContainerView* innerContainerView_;

  // Origin of the arrow at the top of the popup window.
  CGPoint origin_;

  // Model for the data to display.
  std::unique_ptr<PageInfoModel> model_;

  // Thin bridge that pushes model-changed notifications from C++ to Cocoa.
  std::unique_ptr<PageInfoModelObserver> bridge_;

  // Width of the view. Depends on the device (iPad/iPhone).
  CGFloat viewWidth_;

  // Width of the text fields.
  CGFloat textWidth_;

  // YES when the popup has finished animating in. NO otherwise.
  BOOL animateInCompleted_;
}

// Adds the state image at a pre-determined x position and the given y. This
// does not affect the next Y position because the image is placed next to
// a text field that is larger and accounts for the image's size.
- (void)addImageViewForInfo:(const PageInfoModel::SectionInfo&)info
                 toSubviews:(NSMutableArray*)subviews
                   atOffset:(CGFloat)offset;

// Adds the title text field at the given x,y position, and returns the y
// position for the next element.
- (CGFloat)addHeadlineViewForInfo:(const PageInfoModel::SectionInfo&)info
                       toSubviews:(NSMutableArray*)subviews
                          atPoint:(CGPoint)point;

// Adds the description text field at the given x,y position, and returns the y
// position for the next element.
- (CGFloat)addDescriptionViewForInfo:(const PageInfoModel::SectionInfo&)info
                          toSubviews:(NSMutableArray*)subviews
                             atPoint:(CGPoint)point;

// Returns a button with title and action configured for |buttonAction|.
- (UIButton*)buttonForAction:(PageInfoModel::ButtonAction)buttonAction;

// Adds the the button |buttonAction| that explains the icons. Returns the y
// position delta for the next offset.
- (CGFloat)addButton:(PageInfoModel::ButtonAction)buttonAction
          toSubviews:(NSMutableArray*)subviews
            atOffset:(CGFloat)offset;

@property(nonatomic, strong) UIView* containerView;
@property(nonatomic, strong) UIView* popupContainer;
@end

@implementation PageInfoViewController

@synthesize containerView = containerView_;
@synthesize popupContainer = popupContainer_;
@synthesize dispatcher = dispatcher_;

- (id)initWithModel:(PageInfoModel*)model
                  bridge:(PageInfoModelObserver*)bridge
             sourcePoint:(CGPoint)sourcePoint
    presentationProvider:(id<PageInfoPresentation>)provider
              dispatcher:(id<PageInfoCommands, PageInfoReloading>)dispatcher {
  DCHECK(provider);
  self = [super init];
  if (self) {
    scrollView_ =
        [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 240, 128)];
    [scrollView_ setMultipleTouchEnabled:YES];
    [scrollView_ setClipsToBounds:YES];
    [scrollView_ setShowsHorizontalScrollIndicator:NO];
    [scrollView_ setIndicatorStyle:UIScrollViewIndicatorStyleBlack];
    [scrollView_
        setAutoresizingMask:(UIViewAutoresizingFlexibleTrailingMargin() |
                             UIViewAutoresizingFlexibleTopMargin)];

    innerContainerView_ =
        [[BidiContainerView alloc] initWithFrame:CGRectMake(0, 0, 194, 327)];
    [innerContainerView_ setBackgroundColor:[UIColor clearColor]];
    [innerContainerView_
        setAccessibilityLabel:@"Page Security Info Scroll Container"];
    [innerContainerView_
        setAutoresizingMask:(UIViewAutoresizingFlexibleTrailingMargin() |
                             UIViewAutoresizingFlexibleBottomMargin)];

    model_.reset(model);
    bridge_.reset(bridge);
    origin_ = sourcePoint;
    dispatcher_ = dispatcher;

    UIInterfaceOrientation orientation =
        [[UIApplication sharedApplication] statusBarOrientation];
    viewWidth_ = IsCompactWidth() ? kViewWidthCompact : kViewWidthRegular;
    // Special case iPhone landscape.
    if (!IsIPadIdiom() && UIInterfaceOrientationIsLandscape(orientation))
      viewWidth_ = kViewWidthiPhoneLandscape;

    textWidth_ = viewWidth_ - (kImageSize + kImageSpacing + kFramePadding * 2 +
                               kScrollViewInset * 2);

    UILongPressGestureRecognizer* touchDownRecognizer =
        [[UILongPressGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(rootViewTapped:)];
    // Setting the duration to .001 makes this similar to a control event
    // UIControlEventTouchDown.
    [touchDownRecognizer setMinimumPressDuration:.001];
    [touchDownRecognizer setDelegate:self];

    containerView_ = [[UIView alloc] init];
    [containerView_ addGestureRecognizer:touchDownRecognizer];
    [containerView_
        setBackgroundColor:[UIColor colorWithWhite:0 alpha:kShieldAlpha]];
    [containerView_ setOpaque:NO];
    [containerView_ setAlpha:0];
    [containerView_ setAccessibilityViewIsModal:YES];

    popupContainer_ = [[UIView alloc] initWithFrame:CGRectZero];
    [popupContainer_ setBackgroundColor:[UIColor whiteColor]];
    [popupContainer_ setClipsToBounds:YES];
    [containerView_ addSubview:popupContainer_];

    [self.popupContainer addSubview:scrollView_];
    [scrollView_ addSubview:innerContainerView_];
    [scrollView_ setAccessibilityIdentifier:@"Page Security Scroll View"];
    [provider presentPageInfoView:self.containerView];
    [self performLayout];

    [self animatePageInfoViewIn:sourcePoint];
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    containerView_);
  }

  return self;
}

- (void)performLayout {
  CGFloat offset = kInitialLinePadding;

  // Keep the new subviews in an array that gets replaced at the end.
  NSMutableArray* subviews = [NSMutableArray array];

  int sectionCount = model_->GetSectionCount();
  PageInfoModel::ButtonAction action = PageInfoModel::BUTTON_NONE;

  for (int i = 0; i < sectionCount; i++) {
    PageInfoModel::SectionInfo info = model_->GetSectionInfo(i);

    if (action == PageInfoModel::BUTTON_NONE &&
        info.button != PageInfoModel::BUTTON_NONE) {
      // Show the button corresponding to the first section that requires a
      // button.
      action = info.button;
    }

    // Only certain sections have images. This affects the X position.
    BOOL hasImage = model_->GetIconImage(info.icon_id) != nil;
    CGFloat xPosition = (hasImage ? kTextXPosition : kTextXPositionNoImage);

    // Insert the image subview for sections that are appropriate.
    CGFloat imageBaseline = offset + kImageSize;
    if (hasImage) {
      [self addImageViewForInfo:info
                     toSubviews:subviews
                       atOffset:offset + PageInfoImageVerticalOffset()];
    }

    // Add the title.
    if (!info.headline.empty()) {
      offset += [self addHeadlineViewForInfo:info
                                  toSubviews:subviews
                                     atPoint:CGPointMake(xPosition, offset)];
      offset += kHeadlineSpacing;
    }

    // Create the description of the state.
    offset += [self addDescriptionViewForInfo:info
                                   toSubviews:subviews
                                      atPoint:CGPointMake(xPosition, offset)];

    // If at this point the description and optional headline and button are
    // not as tall as the image, adjust the offset by the difference.
    CGFloat imageBaselineDelta = imageBaseline - offset;
    if (imageBaselineDelta > 0)
      offset += imageBaselineDelta;

    // Add the separators.
    int testSectionCount = sectionCount - 1;
    if (i != testSectionCount ||
        (i == testSectionCount && action != PageInfoModel::BUTTON_NONE)) {
      offset += kVerticalSpacing;
    }
  }

  // The last item at the bottom of the window is the help center link. Do not
  // show this for the internal pages, which have one section.
  offset += [self addButton:action toSubviews:subviews atOffset:offset];

  // Add the bottom padding.
  offset += kVerticalSpacing;
  CGRect frame =
      CGRectMake(kInitialFramePosition, origin_.y, viewWidth_, offset);

  // Increase the size of the frame by the amount used for drawing rounded
  // corners and shadow.
  frame.size.height += kPageInfoViewFooterSize;

  if (CGRectGetMaxY(frame) >
      CGRectGetMaxY([[self containerView] superview].bounds) -
          kFrameBottomPadding) {
    // If the frame is bigger than the parent view than change the frame to
    // fit in the superview bounds.
    frame.size.height = [[self containerView] superview].bounds.size.height -
                        kFrameBottomPadding - frame.origin.y;

    [scrollView_ setScrollEnabled:YES];
    [scrollView_ flashScrollIndicators];
  } else {
    [scrollView_ setScrollEnabled:NO];
  }

  CGRect containerBounds = [containerView_ bounds];
  CGRect popupFrame = frame;
  popupFrame.origin.x =
      CGRectGetMidX(containerBounds) - CGRectGetWidth(popupFrame) / 2.0;
  popupFrame.origin.y =
      CGRectGetMidY(containerBounds) - CGRectGetHeight(popupFrame) / 2.0;

  popupFrame.origin = AlignPointToPixel(popupFrame.origin);
  CGRect innerFrame = CGRectMake(0, 0, popupFrame.size.width, offset);

  // If the initial animation has completed, animate the new frames.
  if (animateInCompleted_) {
    [UIView cr_animateWithDuration:ios::material::kDuration3
                             delay:0
                             curve:ios::material::CurveEaseInOut
                           options:0
                        animations:^{
                          [popupContainer_ setFrame:popupFrame];
                          [scrollView_ setFrame:[popupContainer_ bounds]];
                          [innerContainerView_ setFrame:innerFrame];
                        }
                        completion:nil];
  } else {
    // Popup hasn't finished animating in yet. Set frames immediately.
    [popupContainer_ setFrame:popupFrame];
    [scrollView_ setFrame:[popupContainer_ bounds]];
    [innerContainerView_ setFrame:innerFrame];
  }

  for (UIView* view in [innerContainerView_ subviews]) {
    [view removeFromSuperview];
  }

  for (UIView* view in subviews) {
    [innerContainerView_ addSubview:view];
    [innerContainerView_ setSubviewNeedsAdjustmentForRTL:view];
  }

  [scrollView_ setContentSize:innerContainerView_.frame.size];
}

- (void)dismiss {
  [self animatePageInfoViewOut];
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  nil);
}

#pragma mark - Helper methods to create subviews.

- (void)addImageViewForInfo:(const PageInfoModel::SectionInfo&)info
                 toSubviews:(NSMutableArray*)subviews
                   atOffset:(CGFloat)offset {
  CGRect frame = CGRectMake(kFramePadding, offset, kImageSize, kImageSize);
  UIImageView* imageView = [[UIImageView alloc] initWithFrame:frame];
  [imageView setImage:model_->GetIconImage(info.icon_id)->ToUIImage()];
  [subviews addObject:imageView];
}

- (CGFloat)addHeadlineViewForInfo:(const PageInfoModel::SectionInfo&)info
                       toSubviews:(NSMutableArray*)subviews
                          atPoint:(CGPoint)point {
  CGRect frame = CGRectMake(point.x, point.y, textWidth_, kHeadlineHeight);
  UILabel* label = [[UILabel alloc] initWithFrame:frame];
  [label setTextAlignment:NSTextAlignmentNatural];
  [label setText:base::SysUTF16ToNSString(info.headline)];
  [label setTextColor:PageInfoTextColor()];
  [label setFont:PageInfoHeadlineFont()];
  [label setBackgroundColor:[UIColor clearColor]];
  [label setFrame:frame];
  [label setLineBreakMode:NSLineBreakByTruncatingHead];
  [subviews addObject:label];
  return CGRectGetHeight(frame);
}

- (CGFloat)addDescriptionViewForInfo:(const PageInfoModel::SectionInfo&)info
                          toSubviews:(NSMutableArray*)subviews
                             atPoint:(CGPoint)point {
  CGRect frame = CGRectMake(point.x, point.y, textWidth_, kImageSize);
  UILabel* label = [[UILabel alloc] initWithFrame:frame];
  [label setTextAlignment:NSTextAlignmentNatural];
  NSString* description = base::SysUTF16ToNSString(info.description);
  UIFont* font = [MDCTypography captionFont];
  [label setTextColor:PageInfoTextColor()];
  [label setText:description];
  [label setFont:font];
  [label setNumberOfLines:0];
  [label setBackgroundColor:[UIColor clearColor]];

  // If the text is oversized, resize the text field.
  CGSize constraintSize = CGSizeMake(textWidth_, CGFLOAT_MAX);
  CGSize sizeToFit =
      [description cr_boundingSizeWithSize:constraintSize font:font];
  frame.size.height = sizeToFit.height;
  [label setFrame:frame];
  [subviews addObject:label];
  return CGRectGetHeight(frame);
}

- (UIButton*)buttonForAction:(PageInfoModel::ButtonAction)buttonAction {
  if (buttonAction == PageInfoModel::BUTTON_NONE) {
    return nil;
  }
  UIButton* button = [[UIButton alloc] initWithFrame:CGRectZero];
  int messageId;
  NSString* accessibilityID = @"Reload button";
  switch (buttonAction) {
    case PageInfoModel::BUTTON_NONE:
      NOTREACHED();
      return nil;
    case PageInfoModel::BUTTON_SHOW_SECURITY_HELP:
      messageId = IDS_LEARN_MORE;
      accessibilityID = @"Learn more";
      [button addTarget:self.dispatcher
                    action:@selector(showSecurityHelpPage)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    case PageInfoModel::BUTTON_RELOAD:
      messageId = IDS_IOS_PAGE_INFO_RELOAD;
      accessibilityID = @"Reload button";
      [button addTarget:self.dispatcher
                    action:@selector(hidePageInfo)
          forControlEvents:UIControlEventTouchUpInside];
      [button addTarget:self.dispatcher
                    action:@selector(reload)
          forControlEvents:UIControlEventTouchUpInside];
      break;
  };

  NSString* title = l10n_util::GetNSStringWithFixup(messageId);
  SetA11yLabelAndUiAutomationName(button, messageId, accessibilityID);
  [button setTitle:title forState:UIControlStateNormal];
  return button;
}

- (CGFloat)addButton:(PageInfoModel::ButtonAction)buttonAction
          toSubviews:(NSMutableArray*)subviews
            atOffset:(CGFloat)offset {
  UIButton* button = [self buttonForAction:buttonAction];
  if (!button) {
    return 0;
  }
  // The size of the initial frame is irrelevant since it will be changed based
  // on the size for the string inside.
  CGRect frame = CGRectMake(kButtonXOffset, offset, 100, 10);

  UIFont* font = [MDCTypography captionFont];
  CGSize sizeWithFont =
      [[[button titleLabel] text] cr_pixelAlignedSizeWithFont:font];
  frame.size = sizeWithFont;
  // According to iOS Human Interface Guidelines, minimal size of UIButton
  // should be 44x44.
  frame.size.height = std::max<CGFloat>(44, frame.size.height);

  [button setFrame:frame];

  [button.titleLabel setFont:font];
  [button.titleLabel setTextAlignment:NSTextAlignmentLeft];
  [button setTitleColor:PageInfoHelpButtonColor()
               forState:UIControlStateNormal];
  [button setTitleColor:PageInfoHelpButtonColor()
               forState:UIControlStateSelected];
  [button setBackgroundColor:[UIColor clearColor]];

  [subviews addObject:button];

  return CGRectGetHeight([button frame]);
}

#pragma mark - UIGestureRecognizerDelegate Implemenation

- (void)rootViewTapped:(UIGestureRecognizer*)sender {
  CGPoint pt = [sender locationInView:containerView_];
  if (!CGRectContainsPoint([popupContainer_ frame], pt)) {
    [self.dispatcher hidePageInfo];
  }
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  CGPoint pt = [touch locationInView:containerView_];
  return !CGRectContainsPoint([popupContainer_ frame], pt);
}

- (void)animatePageInfoViewIn:(CGPoint)sourcePoint {
  // Animate the info card itself.
  CATransform3D scaleTransform = PageInfoAnimationScale();

  CABasicAnimation* scaleAnimation =
      [CABasicAnimation animationWithKeyPath:@"transform"];
  [scaleAnimation setFromValue:[NSValue valueWithCATransform3D:scaleTransform]];

  CABasicAnimation* positionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  [positionAnimation setFromValue:[NSValue valueWithCGPoint:sourcePoint]];

  CAAnimationGroup* sizeAnimation = [CAAnimationGroup animation];
  [sizeAnimation setAnimations:@[ scaleAnimation, positionAnimation ]];
  [sizeAnimation
      setTimingFunction:TimingFunction(ios::material::CurveEaseInOut)];
  [sizeAnimation setDuration:ios::material::kDuration3];

  CABasicAnimation* fadeAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  [fadeAnimation setTimingFunction:TimingFunction(ios::material::CurveEaseOut)];
  [fadeAnimation setDuration:ios::material::kDuration6];
  [fadeAnimation setFromValue:@0];
  [fadeAnimation setToValue:@1];

  [[popupContainer_ layer] addAnimation:fadeAnimation forKey:@"fade"];
  [[popupContainer_ layer] addAnimation:sizeAnimation forKey:@"size"];

  // Animation the background grey overlay.
  CABasicAnimation* overlayAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  [overlayAnimation
      setTimingFunction:TimingFunction(ios::material::CurveEaseOut)];
  [overlayAnimation setDuration:ios::material::kDuration3];
  [overlayAnimation setFromValue:@0];
  [overlayAnimation setToValue:@1];

  [[containerView_ layer] addAnimation:overlayAnimation forKey:@"fade"];
  [containerView_ setAlpha:1];

  // Animate the contents of the info card.
  CALayer* contentsLayer = [innerContainerView_ layer];

  CGRect startFrame = CGRectOffset([innerContainerView_ frame], 0, -32);
  CAAnimation* contentSlideAnimation = FrameAnimationMake(
      contentsLayer, startFrame, [innerContainerView_ frame]);
  [contentSlideAnimation
      setTimingFunction:TimingFunction(ios::material::CurveEaseOut)];
  [contentSlideAnimation setDuration:ios::material::kDuration5];
  contentSlideAnimation =
      DelayedAnimationMake(contentSlideAnimation, ios::material::kDuration2);
  [contentsLayer addAnimation:contentSlideAnimation forKey:@"slide"];

  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    [innerContainerView_ setAlpha:1];
    animateInCompleted_ = YES;
  }];
  CAAnimation* contentFadeAnimation = OpacityAnimationMake(0.0, 1.0);
  [contentFadeAnimation
      setTimingFunction:TimingFunction(ios::material::CurveEaseOut)];
  [contentFadeAnimation setDuration:ios::material::kDuration5];
  contentFadeAnimation =
      DelayedAnimationMake(contentFadeAnimation, ios::material::kDuration1);
  [contentsLayer addAnimation:contentFadeAnimation forKey:@"fade"];
  [CATransaction commit];

  // Since the animations have delay on them, the alpha of the content view
  // needs to be set to zero and then one after the animation starts. If these
  // steps are not taken, there will be a visible flash/jump from the initial
  // spot during the animation.
  [innerContainerView_ setAlpha:0];
}

- (void)animatePageInfoViewOut {
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    [self.containerView removeFromSuperview];
  }];

  CABasicAnimation* opacityAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  [opacityAnimation
      setTimingFunction:TimingFunction(ios::material::CurveEaseIn)];
  [opacityAnimation setDuration:ios::material::kDuration3];
  [opacityAnimation setFromValue:@1];
  [opacityAnimation setToValue:@0];
  [[containerView_ layer] addAnimation:opacityAnimation forKey:@"animateOut"];

  [popupContainer_ setAlpha:0];
  [containerView_ setAlpha:0];
  [CATransaction commit];
}

@end
