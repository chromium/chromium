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
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#include "ios/chrome/browser/ui/page_info/page_info_model.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_reloading.h"
#import "ios/chrome/browser/ui/util/animation_util.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
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

#pragma mark - PageInfoModelBubbleBridge

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
      base::BindOnce(&PageInfoModelBubbleBridge::PerformLayout,
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

#pragma mark - PageInfoViewController

@interface PageInfoViewController () {
  // Scroll View inside the PageInfoView used to display content that exceeds
  // the available space.
  UIScrollView* _scrollView;
  // Container View added inside the Scroll View. All content is added to this
  // view.
  BidiContainerView* _innerContainerView;

  // Origin of the arrow at the top of the popup window.
  CGPoint _arrowOriginPoint;

  // Model for the data to display.
  std::unique_ptr<PageInfoModel> _model;

  // Thin bridge that pushes model-changed notifications from C++ to Cocoa.
  std::unique_ptr<PageInfoModelObserver> _modelBridge;

  // Width of the view. Depends on the device (iPad/iPhone).
  CGFloat _viewWidth;

  // Width of the text fields.
  CGFloat _textWidth;

  // YES when the popup has finished animating in. NO otherwise.
  BOOL _displayAnimationCompleted;
}

@property(nonatomic, strong) UIView* containerView;
@property(nonatomic, strong) UIView* popupContainer;
// An invisible button added to the |containerView|. Closes the popup just like
// the tap on the background. Exposed purely for voiceover purposes.
@property(nonatomic, strong) UIButton* closeButton;

@end

@implementation PageInfoViewController

#pragma mark public

- (id)initWithModel:(PageInfoModel*)model
                  bridge:(PageInfoModelObserver*)bridge
             sourcePoint:(CGPoint)sourcePoint
    presentationProvider:(id<PageInfoPresentation>)provider
              dispatcher:(id<PageInfoCommands, PageInfoReloading>)dispatcher {
  DCHECK(provider);
  self = [super init];
  if (self) {
    _scrollView =
        [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 240, 128)];
    [_scrollView setMultipleTouchEnabled:YES];
    [_scrollView setClipsToBounds:YES];
    [_scrollView setShowsHorizontalScrollIndicator:NO];
    [_scrollView setIndicatorStyle:UIScrollViewIndicatorStyleBlack];
    [_scrollView
        setAutoresizingMask:(UIViewAutoresizingFlexibleTrailingMargin() |
                             UIViewAutoresizingFlexibleTopMargin)];

    _innerContainerView =
        [[BidiContainerView alloc] initWithFrame:CGRectMake(0, 0, 194, 327)];
    [_innerContainerView
        setAccessibilityLabel:@"Page Security Info Scroll Container"];
    [_innerContainerView
        setAutoresizingMask:(UIViewAutoresizingFlexibleTrailingMargin() |
                             UIViewAutoresizingFlexibleBottomMargin)];

    _model.reset(model);
    _modelBridge.reset(bridge);
    _arrowOriginPoint = sourcePoint;
    _dispatcher = dispatcher;

    UIInterfaceOrientation orientation =
        [[UIApplication sharedApplication] statusBarOrientation];
    _viewWidth = IsCompactWidth() ? kViewWidthCompact : kViewWidthRegular;
    // Special case iPhone landscape.
    if (!IsIPadIdiom() && UIInterfaceOrientationIsLandscape(orientation))
      _viewWidth = kViewWidthiPhoneLandscape;

    _textWidth = _viewWidth - (kImageSize + kImageSpacing + kFramePadding * 2 +
                               kScrollViewInset * 2);

    _containerView = [[UIView alloc] init];
    [_containerView setBackgroundColor:[UIColor colorWithWhite:0
                                                         alpha:kShieldAlpha]];
    [_containerView setOpaque:NO];
    [_containerView setAlpha:0];
    [_containerView setAccessibilityViewIsModal:YES];
    _containerView.accessibilityIdentifier =
        kPageInfoViewAccessibilityIdentifier;

    // Set up an invisible button that closes the popup.
    _closeButton = [[UIButton alloc] init];
    _closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_DONE);
    [_closeButton addTarget:dispatcher
                     action:@selector(hidePageInfo)
           forControlEvents:UIControlEventTouchDown];
    [_containerView addSubview:_closeButton];

    _popupContainer = [[UIView alloc] initWithFrame:CGRectZero];
    [_popupContainer setBackgroundColor:UIColor.cr_systemBackgroundColor];
    [_popupContainer setClipsToBounds:YES];
    _popupContainer.userInteractionEnabled = YES;
    [_containerView addSubview:_popupContainer];

    [self.popupContainer addSubview:_scrollView];
    [_scrollView addSubview:_innerContainerView];
    [_scrollView setAccessibilityIdentifier:@"Page Security Scroll View"];
    [provider presentPageInfoView:self.containerView];
    [self performLayout];

    [self animatePageInfoViewIn:sourcePoint];
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    _containerView);
  }

  return self;
}

- (void)performLayout {
  CGFloat offset = kInitialLinePadding;

  // Keep the new subviews in an array that gets replaced at the end.
  NSMutableArray* subviews = [NSMutableArray array];

  int sectionCount = _model->GetSectionCount();
  PageInfoModel::ButtonAction action = PageInfoModel::BUTTON_NONE;

  for (int i = 0; i < sectionCount; i++) {
    PageInfoModel::SectionInfo info = _model->GetSectionInfo(i);

    if (action == PageInfoModel::BUTTON_NONE &&
        info.button != PageInfoModel::BUTTON_NONE) {
      // Show the button corresponding to the first section that requires a
      // button.
      action = info.button;
    }

    // Only certain sections have images. This affects the X position.
    BOOL hasImage = _model->GetIconImage(info.icon_id) != nil;
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
  CGRect frame = CGRectMake(kInitialFramePosition, _arrowOriginPoint.y,
                            _viewWidth, offset);

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

    [_scrollView setScrollEnabled:YES];
    [_scrollView flashScrollIndicators];
  } else {
    [_scrollView setScrollEnabled:NO];
  }

  CGRect containerBounds = [_containerView bounds];
  CGRect popupFrame = frame;
  popupFrame.origin.x =
      CGRectGetMidX(containerBounds) - CGRectGetWidth(popupFrame) / 2.0;
  popupFrame.origin.y =
      CGRectGetMidY(containerBounds) - CGRectGetHeight(popupFrame) / 2.0;

  popupFrame.origin = AlignPointToPixel(popupFrame.origin);
  CGRect innerFrame = CGRectMake(0, 0, popupFrame.size.width, offset);

  // If the initial animation has completed, animate the new frames.
  if (_displayAnimationCompleted) {
    [UIView cr_animateWithDuration:ios::material::kDuration3
                             delay:0
                             curve:ios::material::CurveEaseInOut
                           options:0
                        animations:^{
                          [_popupContainer setFrame:popupFrame];
                          [_scrollView setFrame:[_popupContainer bounds]];
                          [_innerContainerView setFrame:innerFrame];
                        }
                        completion:nil];
  } else {
    // Popup hasn't finished animating in yet. Set frames immediately.
    [_popupContainer setFrame:popupFrame];
    [_scrollView setFrame:[_popupContainer bounds]];
    [_innerContainerView setFrame:innerFrame];
  }

  for (UIView* view in [_innerContainerView subviews]) {
    [view removeFromSuperview];
  }

  for (UIView* view in subviews) {
    [_innerContainerView addSubview:view];
    [_innerContainerView setSubviewNeedsAdjustmentForRTL:view];
  }

  [_scrollView setContentSize:_innerContainerView.frame.size];
  _closeButton.frame = _containerView.bounds;
}

- (void)dismiss {
  [self animatePageInfoViewOut];
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  nil);
}

#pragma mark - internal

- (BOOL)accessibilityPerformEscape {
  [self.dispatcher hidePageInfo];
  return YES;
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

  [[_popupContainer layer] addAnimation:fadeAnimation forKey:@"fade"];
  [[_popupContainer layer] addAnimation:sizeAnimation forKey:@"size"];

  // Animation the background grey overlay.
  CABasicAnimation* overlayAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  [overlayAnimation
      setTimingFunction:TimingFunction(ios::material::CurveEaseOut)];
  [overlayAnimation setDuration:ios::material::kDuration3];
  [overlayAnimation setFromValue:@0];
  [overlayAnimation setToValue:@1];

  [[_containerView layer] addAnimation:overlayAnimation forKey:@"fade"];
  [_containerView setAlpha:1];

  // Animate the contents of the info card.
  CALayer* contentsLayer = [_innerContainerView layer];

  CGRect startFrame = CGRectOffset([_innerContainerView frame], 0, -32);
  CAAnimation* contentSlideAnimation = FrameAnimationMake(
      contentsLayer, startFrame, [_innerContainerView frame]);
  [contentSlideAnimation
      setTimingFunction:TimingFunction(ios::material::CurveEaseOut)];
  [contentSlideAnimation setDuration:ios::material::kDuration5];
  contentSlideAnimation =
      DelayedAnimationMake(contentSlideAnimation, ios::material::kDuration2);
  [contentsLayer addAnimation:contentSlideAnimation forKey:@"slide"];

  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    [_innerContainerView setAlpha:1];
    _displayAnimationCompleted = YES;
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
  [_innerContainerView setAlpha:0];
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
  [[_containerView layer] addAnimation:opacityAnimation forKey:@"animateOut"];

  [_popupContainer setAlpha:0];
  [_containerView setAlpha:0];
  [CATransaction commit];
}

#pragma mark - Helper methods to create subviews.

// Adds the state image at a pre-determined x position and the given y. This
// does not affect the next Y position because the image is placed next to
// a text field that is larger and accounts for the image's size.
- (void)addImageViewForInfo:(const PageInfoModel::SectionInfo&)info
                 toSubviews:(NSMutableArray*)subviews
                   atOffset:(CGFloat)offset {
  CGRect frame = CGRectMake(kFramePadding, offset, kImageSize, kImageSize);
  UIImageView* imageView = [[UIImageView alloc] initWithFrame:frame];
  imageView.tintColor = UIColor.cr_labelColor;
  UIImage* image = [_model->GetIconImage(info.icon_id)->ToUIImage()
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [imageView setImage:image];
  [subviews addObject:imageView];
}

// Adds the title text field at the given x,y position, and returns the y
// position for the next element.
- (CGFloat)addHeadlineViewForInfo:(const PageInfoModel::SectionInfo&)info
                       toSubviews:(NSMutableArray*)subviews
                          atPoint:(CGPoint)point {
  CGRect frame = CGRectMake(point.x, point.y, _textWidth, kHeadlineHeight);
  UILabel* label = [[UILabel alloc] initWithFrame:frame];
  [label setTextAlignment:NSTextAlignmentNatural];
  [label setText:base::SysUTF16ToNSString(info.headline)];
  [label setTextColor:UIColor.cr_labelColor];
  [label setFont:PageInfoHeadlineFont()];
  [label setFrame:frame];
  [label setLineBreakMode:NSLineBreakByTruncatingHead];
  [subviews addObject:label];
  return CGRectGetHeight(frame);
}

// Adds the description text field at the given x,y position, and returns the y
// position for the next element.
- (CGFloat)addDescriptionViewForInfo:(const PageInfoModel::SectionInfo&)info
                          toSubviews:(NSMutableArray*)subviews
                             atPoint:(CGPoint)point {
  CGRect frame = CGRectMake(point.x, point.y, _textWidth, kImageSize);
  UILabel* label = [[UILabel alloc] initWithFrame:frame];
  [label setTextAlignment:NSTextAlignmentNatural];
  NSString* description = base::SysUTF16ToNSString(info.description);
  UIFont* font = [MDCTypography captionFont];
  [label setTextColor:UIColor.cr_labelColor];
  [label setText:description];
  [label setFont:font];
  [label setNumberOfLines:0];

  // If the text is oversized, resize the text field.
  CGSize constraintSize = CGSizeMake(_textWidth, CGFLOAT_MAX);
  CGSize sizeToFit =
      [description cr_boundingSizeWithSize:constraintSize font:font];
  frame.size.height = sizeToFit.height;
  [label setFrame:frame];
  [subviews addObject:label];
  return CGRectGetHeight(frame);
}

// Returns a button with title and action configured for |buttonAction|.
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

// Adds the the button |buttonAction| that explains the icons. Returns the y
// position delta for the next offset.
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
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateSelected];

  [subviews addObject:button];

  return CGRectGetHeight([button frame]);
}

@end
