// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#include "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/util/CRUILabel+AttributeUtils.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/label_observer.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// An enum type to describe size classes.
typedef NS_ENUM(NSInteger, SizeClassIdiom) {
  COMPACT = 0,
  REGULAR,
  UNSPECIFIED,
  SIZE_CLASS_COUNT = UNSPECIFIED,
};

// Returns the SizeClassIdiom corresponding with |size_class|.
SizeClassIdiom GetSizeClassIdiom(UIUserInterfaceSizeClass size_class) {
  switch (size_class) {
    case UIUserInterfaceSizeClassCompact:
      return COMPACT;
    case UIUserInterfaceSizeClassRegular:
      return REGULAR;
    case UIUserInterfaceSizeClassUnspecified:
      return UNSPECIFIED;
  }
}

// Accessibility identifier for the checkbox button.
NSString* const kUMAMetricsButtonAccessibilityIdentifier =
    @"UMAMetricsButtonAccessibilityIdentifier";

// The width of the container view for a REGULAR width size class.
const CGFloat kContainerViewRegularWidth = 510.0;

// The percentage of the view's width taken up by the container view for a
// COMPACT width size class.
const CGFloat kContainerViewCompactWidthPercentage = 0.8;

// Layout constants.
const CGFloat kImageTopPadding[SIZE_CLASS_COUNT] = {32.0, 50.0};
const CGFloat kTOSLabelTopPadding[SIZE_CLASS_COUNT] = {34.0, 40.0};
const CGFloat kOptInLabelPadding[SIZE_CLASS_COUNT] = {10.0, 14.0};
const CGFloat kCheckBoxPadding[SIZE_CLASS_COUNT] = {10.0, 16.0};
const CGFloat kOKButtonBottomPadding[SIZE_CLASS_COUNT] = {32.0, 32.0};
const CGFloat kOKButtonHeight[SIZE_CLASS_COUNT] = {36.0, 54.0};
// Multiplier matches that used in LaunchScreen.xib to determine size of logo.
const CGFloat kAppLogoProportionMultiplier = 0.381966;

// Font sizes.
const CGFloat kTitleLabelFontSize[SIZE_CLASS_COUNT] = {24.0, 36.0};
const CGFloat kTOSLabelFontSize[SIZE_CLASS_COUNT] = {14.0, 21.0};
const CGFloat kTOSLabelLineHeight[SIZE_CLASS_COUNT] = {20.0, 32.0};
const CGFloat kOptInLabelFontSize[SIZE_CLASS_COUNT] = {13.0, 19.0};
const CGFloat kOptInLabelLineHeight[SIZE_CLASS_COUNT] = {18.0, 26.0};
const CGFloat kOKButtonTitleLabelFontSize[SIZE_CLASS_COUNT] = {14.0, 20.0};

// Animation constants
const CGFloat kAnimationDuration = .4;
// Delay animation to avoid interaction with launch screen fadeout.
const CGFloat kAnimationDelay = .5;

// Image names.
NSString* const kAppLogoImageName = @"launchscreen_app_logo";
NSString* const kCheckBoxImageName = @"checkbox";
NSString* const kCheckBoxCheckedImageName = @"checkbox_checked";

// Constants for the Terms of Service and Privacy Notice URLs in the
// first run experience.
const char kTermsOfServiceUrl[] = "internal://terms-of-service";
const char kPrivacyNoticeUrl[] = "internal://privacy-notice";

}  // namespace

@interface WelcomeToChromeView () {
  UIView* _containerView;
  UILabel* _titleLabel;
  UIImageView* _imageView;
  UILabel* _TOSLabel;
  LabelLinkController* _TOSLabelLinkController;
  UIButton* _checkBoxButton;
  UILabel* _optInLabel;
  PrimaryActionButton* _OKButton;
}

// Subview properties are lazily instantiated upon their first use.

// A container view used to layout and center subviews.
@property(strong, nonatomic, readonly) UIView* containerView;
// The "Welcome to Chrome" label that appears at the top of the view.
@property(strong, nonatomic, readonly) UILabel* titleLabel;
// The Chrome logo image view.
@property(strong, nonatomic, readonly) UIImageView* imageView;
// The "Terms of Service" label.
@property(strong, nonatomic, readonly) UILabel* TOSLabel;
// Observer for setting the size of the TOSLabel with cr_lineHeight.
@property(strong, nonatomic) LabelObserver* TOSObserver;
// The stats reporting opt-in label.
@property(strong, nonatomic, readonly) UILabel* optInLabel;
// Observer for setting the size of the optInLabel with cr_lineHeight.
@property(strong, nonatomic) LabelObserver* optInObserver;
// The stats reporting opt-in checkbox button.
@property(strong, nonatomic, readonly) UIButton* checkBoxButton;
// The "Accept & Continue" button.
@property(strong, nonatomic, readonly) PrimaryActionButton* OKButton;

// Subview layout methods.  They must be called in the order declared here, as
// subsequent subview layouts depend on the layouts that precede them.
- (void)layoutTitleLabel;
- (void)layoutImageView;
- (void)layoutTOSLabel;
- (void)layoutOptInLabel;
- (void)layoutCheckBoxButton;
- (void)layoutContainerView;
- (void)layoutOKButton;

// Calls the subview configuration selectors below.
- (void)configureSubviews;

// Subview configuration methods.
- (void)configureTitleLabel;
- (void)configureImageView;
- (void)configureTOSLabel;
- (void)configureOptInLabel;
- (void)configureContainerView;
- (void)configureOKButton;

// Action triggered by the check box button.
- (void)checkBoxButtonWasTapped;

// Action triggered by the ok button.
- (void)OKButtonWasTapped;

@end

@implementation WelcomeToChromeView

@synthesize delegate = _delegate;
@synthesize TOSObserver = _TOSObserver;
@synthesize optInObserver = _optInObserver;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

- (void)runLaunchAnimation {
  // Prepare for animation by making views (except for the logo) transparent
  // and finding the initial and final location of the logo.
  self.titleLabel.alpha = 0.0;
  self.TOSLabel.alpha = 0.0;
  self.optInLabel.alpha = 0.0;
  self.checkBoxButton.alpha = 0.0;
  self.OKButton.alpha = 0.0;

  // Get final location of logo based on result from previously run
  // layoutSubviews.
  CGRect finalLogoFrame = self.imageView.frame;
  // Ensure that frame position is valid and that layoutSubviews ran
  // before this method.
  DCHECK(finalLogoFrame.origin.x >= 0 && finalLogoFrame.origin.y >= 0);
  self.imageView.center = CGPointMake(CGRectGetMidX(self.containerView.bounds),
                                      CGRectGetMidY(self.containerView.bounds));

  __weak WelcomeToChromeView* weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
                        delay:kAnimationDelay
                      options:UIViewAnimationCurveEaseInOut
                   animations:^{
                     [weakSelf imageView].frame = finalLogoFrame;
                     [weakSelf titleLabel].alpha = 1.0;
                     [weakSelf TOSLabel].alpha = 1.0;
                     [weakSelf optInLabel].alpha = 1.0;
                     [weakSelf checkBoxButton].alpha = 1.0;
                     [weakSelf OKButton].alpha = 1.0;
                   }
                   completion:nil];
}

- (void)dealloc {
  [self.TOSObserver stopObserving];
  [self.optInObserver stopObserving];
}

#pragma mark - Accessors

- (BOOL)isCheckBoxSelected {
  return self.checkBoxButton.selected;
}

- (void)setCheckBoxSelected:(BOOL)checkBoxSelected {
  if (checkBoxSelected != self.checkBoxButton.selected)
    [self checkBoxButtonWasTapped];
}

- (UIView*)containerView {
  if (!_containerView) {
    _containerView = [[UIView alloc] initWithFrame:CGRectZero];
  }
  return _containerView;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    [_titleLabel setNumberOfLines:0];
    [_titleLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [_titleLabel setBaselineAdjustment:UIBaselineAdjustmentAlignBaselines];
    [_titleLabel
        setText:l10n_util::GetNSString(IDS_IOS_FIRSTRUN_WELCOME_TO_CHROME)];
  }
  return _titleLabel;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    UIImage* image = [UIImage imageNamed:kAppLogoImageName];
    _imageView = [[UIImageView alloc] initWithImage:image];
  }
  return _imageView;
}

- (UILabel*)TOSLabel {
  if (!_TOSLabel) {
    _TOSLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    // Add an observer to the label to be able to keep the cr_lineHeight.
    self.TOSObserver = [LabelObserver observerForLabel:_TOSLabel];
    [self.TOSObserver startObserving];

    [_TOSLabel setNumberOfLines:0];
    [_TOSLabel setTextAlignment:NSTextAlignmentCenter];
  }
  return _TOSLabel;
}

- (UILabel*)optInLabel {
  if (!_optInLabel) {
    _optInLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    // Add an observer to the label to be able to keep the cr_lineHeight.
    self.optInObserver = [LabelObserver observerForLabel:_optInLabel];
    [self.optInObserver startObserving];

    [_optInLabel setNumberOfLines:0];
    [_optInLabel
        setText:l10n_util::GetNSString(IDS_IOS_FIRSTRUN_NEW_OPT_IN_LABEL)];
    [_optInLabel setTextAlignment:NSTextAlignmentNatural];
  }
  return _optInLabel;
}

- (UIButton*)checkBoxButton {
  if (!_checkBoxButton) {
    _checkBoxButton = [[UIButton alloc] initWithFrame:CGRectZero];
    [_checkBoxButton setBackgroundColor:[UIColor clearColor]];
    [_checkBoxButton addTarget:self
                        action:@selector(checkBoxButtonWasTapped)
              forControlEvents:UIControlEventTouchUpInside];
    SetA11yLabelAndUiAutomationName(_checkBoxButton,
                                    IDS_IOS_FIRSTRUN_NEW_OPT_IN_LABEL,
                                    kUMAMetricsButtonAccessibilityIdentifier);
    [_checkBoxButton
        setAccessibilityValue:l10n_util::GetNSString(IDS_IOS_SETTING_OFF)];
    [_checkBoxButton setImage:[UIImage imageNamed:kCheckBoxImageName]
                     forState:UIControlStateNormal];
    UIImage* selectedImage = [[UIImage imageNamed:kCheckBoxCheckedImageName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [_checkBoxButton setImage:selectedImage forState:UIControlStateSelected];
  }
  return _checkBoxButton;
}

- (PrimaryActionButton*)OKButton {
  if (!_OKButton) {
    _OKButton = [[PrimaryActionButton alloc] initWithFrame:CGRectZero];
    [_OKButton addTarget:self
                  action:@selector(OKButtonWasTapped)
        forControlEvents:UIControlEventTouchUpInside];
    NSString* acceptAndContinue =
        l10n_util::GetNSString(IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON);
    [_OKButton setTitle:acceptAndContinue forState:UIControlStateNormal];
    [_OKButton setTitle:acceptAndContinue forState:UIControlStateHighlighted];
    // UIAutomation tests look for the Accept button to skip through the
    // First Run UI when it shows up.
    SetA11yLabelAndUiAutomationName(
        _OKButton, IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON, @"Accept & Continue");
  }
  return _OKButton;
}

#pragma mark - Layout

- (void)willMoveToSuperview:(nullable UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  // Early return if the view hierarchy is already built.
  if (self.containerView.superview) {
    DCHECK_EQ(self, self.containerView.superview);
    return;
  }

  [self addSubview:self.containerView];
  [self.containerView addSubview:self.titleLabel];
  [self.containerView addSubview:self.imageView];
  [self.containerView addSubview:self.TOSLabel];
  [self.containerView addSubview:self.optInLabel];
  [self.containerView addSubview:self.checkBoxButton];
  [self addSubview:self.OKButton];
  [self configureSubviews];
}

- (void)safeAreaInsetsDidChange {
  [super safeAreaInsetsDidChange];
  [self layoutOKButtonAndContainerView];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self layoutTitleLabel];
  [self layoutImageView];
  [self layoutTOSLabel];
  [self layoutOptInLabel];
  [self layoutCheckBoxButton];
  [self layoutOKButtonAndContainerView];
}

- (void)layoutOKButtonAndContainerView {
  // The OK Button must be laid out before the container view so that the
  // container view can take its position into account.
  [self layoutOKButton];
  [self layoutContainerView];
}

- (void)layoutTitleLabel {
  // The label is centered and top-aligned with the container view.
  CGSize containerSize = self.containerView.bounds.size;
  containerSize.height = CGFLOAT_MAX;
  CGSize titleLabelSize = [self.titleLabel sizeThatFits:containerSize];
  self.titleLabel.frame = AlignRectOriginAndSizeToPixels(
      CGRectMake((containerSize.width - titleLabelSize.width) / 2.0, 0.0,
                 titleLabelSize.width, titleLabelSize.height));
}

- (void)layoutImageView {
  // The image is centered and laid out below |titleLabel| as specified by
  // kImageTopPadding.
  CGSize imageViewSize = self.imageView.bounds.size;
  CGFloat imageViewTopPadding = kImageTopPadding[[self heightSizeClassIdiom]];
  self.imageView.frame = AlignRectOriginAndSizeToPixels(CGRectMake(
      (CGRectGetWidth(self.containerView.bounds) - imageViewSize.width) / 2.0,
      CGRectGetMaxY(self.titleLabel.frame) + imageViewTopPadding,
      imageViewSize.width, imageViewSize.height));
}

- (void)layoutTOSLabel {
  // The TOS label is centered and laid out below |imageView| as specified by
  // kTOSLabelTopPadding.
  CGSize containerSize = self.containerView.bounds.size;
  containerSize.height = CGFLOAT_MAX;
  self.TOSLabel.frame = {CGPointZero, containerSize};
  NSString* TOSText = l10n_util::GetNSString(IDS_IOS_FIRSTRUN_AGREE_TO_TERMS);
  NSRange tosLinkTextRange = NSMakeRange(NSNotFound, 0);
  NSRange privacyLinkTextRange = NSMakeRange(NSNotFound, 0);
  TOSText = ParseStringWithTag(TOSText, &tosLinkTextRange,
                               @"BEGIN_LINK_TOS[ \t]*", @"[ \t]*END_LINK_TOS");
  TOSText = ParseStringWithTag(TOSText, &privacyLinkTextRange,
                               @"BEGIN_LINK_PRIVACY[ \t]*",
                               @"[ \t]*END_LINK_PRIVACY");

  DCHECK_NE(NSNotFound, static_cast<NSInteger>(tosLinkTextRange.location));
  DCHECK_NE(0u, tosLinkTextRange.length);
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(privacyLinkTextRange.location));
  DCHECK_NE(0u, privacyLinkTextRange.length);

  self.TOSLabel.text = TOSText;
  if (FixOrphanWord(self.TOSLabel)) {
    // If a newline is inserted, check whether it was added mid-link and adjust
    // |tosLinkTextRange| and |privacyLinkTextRange| accordingly.
    NSRange newlineRange = [self.TOSLabel.text rangeOfString:@"\n"
                                                     options:0
                                                       range:tosLinkTextRange];
    if (newlineRange.length) {
      tosLinkTextRange.location++;
      privacyLinkTextRange.location++;
    }
    newlineRange = [self.TOSLabel.text rangeOfString:@"\n"
                                             options:0
                                               range:privacyLinkTextRange];
    if (newlineRange.length)
      privacyLinkTextRange.location++;
  }

  __weak WelcomeToChromeView* weakSelf = self;
  ProceduralBlockWithURL action = ^(const GURL& url) {
    WelcomeToChromeView* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    if (url == kTermsOfServiceUrl) {
      [[strongSelf delegate] welcomeToChromeViewDidTapTOSLink];
    } else if (url == kPrivacyNoticeUrl) {
      [[strongSelf delegate] welcomeToChromeViewDidTapPrivacyLink];
    } else {
      NOTREACHED();
    }
  };

  _TOSLabelLinkController =
      [[LabelLinkController alloc] initWithLabel:_TOSLabel action:action];
  [_TOSLabelLinkController addLinkWithRange:tosLinkTextRange
                                        url:GURL(kTermsOfServiceUrl)];
  [_TOSLabelLinkController addLinkWithRange:privacyLinkTextRange
                                        url:GURL(kPrivacyNoticeUrl)];
  [_TOSLabelLinkController setLinkColor:[UIColor colorNamed:kBlueColor]];

  CGSize TOSLabelSize = [self.TOSLabel sizeThatFits:containerSize];
  CGFloat TOSLabelTopPadding = kTOSLabelTopPadding[[self heightSizeClassIdiom]];
  self.TOSLabel.frame = AlignRectOriginAndSizeToPixels(
      CGRectMake((containerSize.width - TOSLabelSize.width) / 2.0,
                 CGRectGetMaxY(self.imageView.frame) + TOSLabelTopPadding,
                 TOSLabelSize.width, TOSLabelSize.height));
}

- (void)layoutOptInLabel {
  // The opt in label is laid out to the right (or left in RTL) of the check box
  // button and below |TOSLabel| as specified by kOptInLabelPadding.
  CGSize checkBoxSize =
      [self.checkBoxButton imageForState:self.checkBoxButton.state].size;
  CGFloat checkBoxPadding = kCheckBoxPadding[[self widthSizeClassIdiom]];
  CGFloat optInLabelSidePadding = checkBoxSize.width + 2.0 * checkBoxPadding;
  CGSize optInLabelSize = [self.optInLabel
      sizeThatFits:CGSizeMake(CGRectGetWidth(self.containerView.bounds) -
                                  optInLabelSidePadding,
                              CGFLOAT_MAX)];
  CGFloat optInLabelTopPadding =
      kOptInLabelPadding[[self heightSizeClassIdiom]];
  CGFloat optInLabelOriginX =
      base::i18n::IsRTL() ? 0.0f : optInLabelSidePadding;
  self.optInLabel.frame = AlignRectOriginAndSizeToPixels(
      CGRectMake(optInLabelOriginX,
                 CGRectGetMaxY(self.TOSLabel.frame) + optInLabelTopPadding,
                 optInLabelSize.width, optInLabelSize.height));
  FixOrphanWord(self.optInLabel);
}

- (void)layoutCheckBoxButton {
  // The checkBoxButton is laid out to the left of |optInLabel|.  The view
  // itself is sized so that it covers the label, and the image insets are
  // chosen such that the check box image is centered vertically with
  // |optInLabel|.
  CGSize checkBoxSize =
      [self.checkBoxButton imageForState:self.checkBoxButton.state].size;
  CGFloat checkBoxPadding = kCheckBoxPadding[[self widthSizeClassIdiom]];
  CGSize checkBoxButtonSize =
      CGSizeMake(CGRectGetWidth(self.optInLabel.frame) + checkBoxSize.width +
                     2.0 * checkBoxPadding,
                 std::max(CGRectGetHeight(self.optInLabel.frame),
                          checkBoxSize.height + 2.0f * checkBoxPadding));
  self.checkBoxButton.frame = AlignRectOriginAndSizeToPixels(CGRectMake(
      0.0f,
      CGRectGetMidY(self.optInLabel.frame) - checkBoxButtonSize.height / 2.0,
      checkBoxButtonSize.width, checkBoxButtonSize.height));
  CGFloat largeHorizontalInset =
      checkBoxButtonSize.width - checkBoxSize.width - checkBoxPadding;
  CGFloat smallHorizontalInset = checkBoxPadding;
  self.checkBoxButton.imageEdgeInsets = UIEdgeInsetsMake(
      (checkBoxButtonSize.height - checkBoxSize.height) / 2.0,
      base::i18n::IsRTL() ? largeHorizontalInset : smallHorizontalInset,
      (checkBoxButtonSize.height - checkBoxSize.height) / 2.0,
      base::i18n::IsRTL() ? smallHorizontalInset : largeHorizontalInset);
}

- (void)layoutContainerView {
  // The container view is resized according to the final layout of
  // |checkBoxButton|, which is its lowest subview.  The resized view is then
  // centered horizontally and vertically. If necessary, it is shifted up to
  // allow |kOptInLabelPadding| between |optInLabel| and |OKButton|.
  CGSize containerViewSize = self.containerView.bounds.size;
  containerViewSize.height = CGRectGetMaxY(self.checkBoxButton.frame);

  CGFloat padding = kOptInLabelPadding[[self heightSizeClassIdiom]];
  CGFloat originY = fmin(
      (CGRectGetHeight(self.bounds) - containerViewSize.height) / 2.0,
      CGRectGetMinY(self.OKButton.frame) - padding - containerViewSize.height);

  self.containerView.frame = AlignRectOriginAndSizeToPixels(CGRectMake(
      (CGRectGetWidth(self.bounds) - containerViewSize.width) / 2.0, originY,
      containerViewSize.width, CGRectGetMaxY(self.checkBoxButton.frame)));
}

- (void)layoutOKButton {
  // The OK button is laid out at the bottom of the view as specified by
  // kOKButtonBottomPadding.
  CGFloat OKButtonBottomPadding =
      kOKButtonBottomPadding[[self widthSizeClassIdiom]];
  CGSize OKButtonSize = self.OKButton.bounds.size;
  CGFloat bottomSafeArea = self.safeAreaInsets.bottom;
  self.OKButton.frame = AlignRectOriginAndSizeToPixels(
      CGRectMake((CGRectGetWidth(self.bounds) - OKButtonSize.width) / 2.0,
                 CGRectGetMaxY(self.bounds) - OKButtonSize.height -
                     OKButtonBottomPadding - bottomSafeArea,
                 OKButtonSize.width, OKButtonSize.height));
}

- (void)traitCollectionDidChange:
    (nullable UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self configureSubviews];
}

- (void)configureSubviews {
  [self configureContainerView];
  [self configureTitleLabel];
  [self configureImageView];
  [self configureTOSLabel];
  [self configureOptInLabel];
  [self configureOKButton];
  [self setNeedsLayout];
}

- (void)configureTitleLabel {
  self.titleLabel.font = [[MDCTypography fontLoader]
      regularFontOfSize:kTitleLabelFontSize[[self widthSizeClassIdiom]]];
}

- (void)configureImageView {
  CGFloat sideLength = self.imageView.image.size.width;
  if ([self widthSizeClassIdiom] == COMPACT) {
    sideLength = self.bounds.size.width * kAppLogoProportionMultiplier;
  } else if ([self heightSizeClassIdiom] == COMPACT) {
    sideLength = self.bounds.size.height * kAppLogoProportionMultiplier;
  }
  self.imageView.bounds = AlignRectOriginAndSizeToPixels(
      CGRectMake(self.imageView.bounds.origin.x, self.imageView.bounds.origin.y,
                 sideLength, sideLength));
}

- (void)configureTOSLabel {
  self.TOSLabel.font = [[MDCTypography fontLoader]
      regularFontOfSize:kTOSLabelFontSize[[self widthSizeClassIdiom]]];
  self.TOSLabel.cr_lineHeight = kTOSLabelLineHeight[[self widthSizeClassIdiom]];
}

- (void)configureOptInLabel {
  self.optInLabel.font = [[MDCTypography fontLoader]
      regularFontOfSize:kOptInLabelFontSize[[self widthSizeClassIdiom]]];
  self.optInLabel.cr_lineHeight =
      kOptInLabelLineHeight[[self widthSizeClassIdiom]];
}

- (void)configureContainerView {
  CGFloat containerViewWidth =
      [self widthSizeClassIdiom] == COMPACT
          ? kContainerViewCompactWidthPercentage * CGRectGetWidth(self.bounds)
          : kContainerViewRegularWidth;
  self.containerView.frame =
      CGRectMake(0.0, 0.0, containerViewWidth, CGFLOAT_MAX);
}

- (void)configureOKButton {
  UIFont* font = [[MDCTypography fontLoader]
      mediumFontOfSize:kOKButtonTitleLabelFontSize[[self widthSizeClassIdiom]]];
  [self.OKButton setTitleFont:font forState:UIControlStateNormal];
  CGSize size = [self.OKButton
      sizeThatFits:CGSizeMake(CGFLOAT_MAX,
                              kOKButtonHeight[[self widthSizeClassIdiom]])];
  [self.OKButton
      setBounds:CGRectMake(0, 0, size.width,
                           kOKButtonHeight[[self widthSizeClassIdiom]])];
}

- (SizeClassIdiom)widthSizeClassIdiom {
  UIWindow* keyWindow = [UIApplication sharedApplication].keyWindow;
  UIUserInterfaceSizeClass sizeClass = self.traitCollection.horizontalSizeClass;
  if (sizeClass == UIUserInterfaceSizeClassUnspecified)
    sizeClass = keyWindow.traitCollection.horizontalSizeClass;
  return GetSizeClassIdiom(sizeClass);
}

- (SizeClassIdiom)heightSizeClassIdiom {
  UIWindow* keyWindow = [UIApplication sharedApplication].keyWindow;
  UIUserInterfaceSizeClass sizeClass = self.traitCollection.verticalSizeClass;
  if (sizeClass == UIUserInterfaceSizeClassUnspecified)
    sizeClass = keyWindow.traitCollection.verticalSizeClass;
  return GetSizeClassIdiom(sizeClass);
}

#pragma mark -

- (void)checkBoxButtonWasTapped {
  self.checkBoxButton.selected = !self.checkBoxButton.selected;
  self.checkBoxButton.accessibilityValue =
      self.checkBoxButton.selected
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
}

- (void)OKButtonWasTapped {
  [self.delegate welcomeToChromeViewDidTapOKButton:self];
}

@end
