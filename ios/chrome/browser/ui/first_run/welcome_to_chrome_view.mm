// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view.h"

#import <MaterialComponents/MaterialTypography.h>

#include <ostream>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/ios/ns_range.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/ui/elements/text_view_selection_disabled.h"
#include "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#include "ios/chrome/browser/ui/first_run/first_run_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
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

// Returns the SizeClassIdiom corresponding with `size_class`.
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

// Sets the line height for `label` via attributed text.
void SetLabelLineHeight(UILabel* label, CGFloat line_height) {
  NSMutableAttributedString* updated_text = [label.attributedText mutableCopy];
  if (updated_text.length == 0)
    return;

  NSParagraphStyle* style =
      [updated_text attribute:NSParagraphStyleAttributeName
                      atIndex:0
               effectiveRange:nullptr];
  if (!style)
    style = [NSParagraphStyle defaultParagraphStyle];

  NSMutableParagraphStyle* updated_style = [style mutableCopy];
  [updated_style setMinimumLineHeight:line_height];
  [updated_style setMaximumLineHeight:line_height];
  [updated_text addAttribute:NSParagraphStyleAttributeName
                       value:updated_style
                       range:NSMakeRange(0, [updated_text length])];

  label.attributedText = updated_text;
}

// Tags used to embed TOS link.
NSString* const kBeginTOSLinkTag = @"BEGIN_LINK_TOS[ \t]*";
NSString* const kEndTOSLinkTag = @"[ \t]*END_LINK_TOS";

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
const CGFloat kTOSTextViewTopPadding[SIZE_CLASS_COUNT] = {34.0, 40.0};
const CGFloat kOptInLabelPadding[SIZE_CLASS_COUNT] = {10.0, 14.0};
const CGFloat kCheckBoxPadding[SIZE_CLASS_COUNT] = {10.0, 16.0};
const CGFloat kManagedLabelPadding[SIZE_CLASS_COUNT] = {14.0, 20.0};
const CGFloat kOKButtonBottomPadding[SIZE_CLASS_COUNT] = {32.0, 32.0};
const CGFloat kOKButtonHeight[SIZE_CLASS_COUNT] = {36.0, 54.0};
// Multiplier matches that used in LaunchScreen.xib to determine size of logo.
const CGFloat kAppLogoProportionMultiplier = 0.381966;

// Font sizes.
const CGFloat kTitleLabelFontSize[SIZE_CLASS_COUNT] = {24.0, 36.0};
const CGFloat kTOSTOSTextViewFontSize[SIZE_CLASS_COUNT] = {14.0, 21.0};
const CGFloat kOptInLabelFontSize[SIZE_CLASS_COUNT] = {13.0, 19.0};
const CGFloat kOptInLabelLineHeight[SIZE_CLASS_COUNT] = {18.0, 26.0};
const CGFloat kManagedLabelFontSize[SIZE_CLASS_COUNT] = {13.0, 19.0};
const CGFloat kManagedLabelLineHeight[SIZE_CLASS_COUNT] = {18.0, 26.0};
const CGFloat kOKButtonTitleLabelFontSize[SIZE_CLASS_COUNT] = {14.0, 20.0};

// Animation constants
const CGFloat kAnimationDuration = .4;
// Delay animation to avoid interaction with launch screen fadeout.
const CGFloat kAnimationDelay = .5;

// Image names.
NSString* const kAppLogoImageName = @"launchscreen_app_logo";
NSString* const kCheckBoxImageName = @"checkbox";
NSString* const kCheckBoxCheckedImageName = @"checkbox_checked";
NSString* const kEnterpriseIconImageName = @"enterprise_icon";

// Constant for the Terms of Service URL in the first run experience.
const char kTermsOfServiceUrl[] = "internal://terms-of-service";

}  // namespace

@interface WelcomeToChromeView () <UITextViewDelegate> {
  UIView* _containerView;
  UILabel* _titleLabel;
  UIImageView* _imageView;
  UIButton* _checkBoxButton;
  UILabel* _optInLabel;
  UILabel* _managedLabel;
  UIImageView* _enterpriseIcon;
  PrimaryActionButton* _OKButton;
}

// Subview properties are lazily instantiated upon their first use.
// A container view used to layout and center subviews.
@property(strong, nonatomic, readonly) UIView* containerView;
// The "Welcome to Chrome" label that appears at the top of the view.
@property(strong, nonatomic, readonly) UILabel* titleLabel;
// The Chrome logo image view.
@property(strong, nonatomic, readonly) UIImageView* imageView;
// The "Terms of Service" text view.
@property(strong, nonatomic) TextViewSelectionDisabled* TOSTextView;
// The stats reporting opt-in label.
@property(strong, nonatomic, readonly) UILabel* optInLabel;
// The stats reporting opt-in checkbox button.
@property(strong, nonatomic, readonly) UIButton* checkBoxButton;
// The Chrome logo image view.
@property(strong, nonatomic, readonly) UIImageView* enterpriseIcon;
// The "Accept & Continue" button.
@property(strong, nonatomic, readonly) PrimaryActionButton* OKButton;

// Subview layout methods.  They must be called in the order declared here, as
// subsequent subview layouts depend on the layouts that precede them.
- (void)layoutTitleLabel;
- (void)layoutImageView;
- (void)layoutTOSTextView;
- (void)layoutOptInLabel;
- (void)layoutCheckBoxButton;
- (void)layoutManagedLabel;
- (void)layoutEnterpriseIcon;
- (void)layoutContainerView;
- (void)layoutOKButton;

// Calls the subview configuration selectors below.
- (void)configureSubviews;

// Subview configuration methods.
- (void)configureTitleLabel;
- (void)configureImageView;
- (void)configureTOSTextView;
- (void)configureOptInLabel;
- (void)configureManagedLabel;
- (void)configureContainerView;
- (void)configureOKButton;

// Action triggered by the check box button.
- (void)checkBoxButtonWasTapped;

// Action triggered by the ok button.
- (void)OKButtonWasTapped;

@end

@implementation WelcomeToChromeView

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
  self.TOSTextView.alpha = 0.0;
  self.optInLabel.alpha = 0.0;
  self.checkBoxButton.alpha = 0.0;
  self.managedLabel.alpha = 0.0;
  self.enterpriseIcon.alpha = 0.0;
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
                     [weakSelf TOSTextView].alpha = 1.0;
                     [weakSelf optInLabel].alpha = 1.0;
                     [weakSelf managedLabel].alpha = 1.0;
                     [weakSelf enterpriseIcon].alpha = 1.0;
                     [weakSelf checkBoxButton].alpha = 1.0;
                     [weakSelf OKButton].alpha = 1.0;
                   }
                   completion:nil];
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

- (TextViewSelectionDisabled*)TOSTextView {
  if (!_TOSTextView) {
    _TOSTextView = [TextViewSelectionDisabled textView];
  }
  return _TOSTextView;
}

- (UILabel*)optInLabel {
  if (!_optInLabel) {
    _optInLabel = [[UILabel alloc] initWithFrame:CGRectZero];
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

- (UILabel*)managedLabel {
  if (!_managedLabel) {
    _managedLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    [_managedLabel setNumberOfLines:0];
    [_managedLabel
        setText:l10n_util::GetNSString(IDS_IOS_FIRSTRUN_BROWSER_MANAGED)];
    [_managedLabel setTextAlignment:NSTextAlignmentNatural];
  }
  return _managedLabel;
}

- (UIImageView*)enterpriseIcon {
  if (!_enterpriseIcon) {
    UIImage* image = [UIImage imageNamed:kEnterpriseIconImageName];
    _enterpriseIcon = [[UIImageView alloc] initWithImage:image];
  }
  return _enterpriseIcon;
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

- (BOOL)isBrowserManaged {
  return [[[NSUserDefaults standardUserDefaults]
             dictionaryForKey:kPolicyLoaderIOSConfigurationKey] count] > 0;
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
  [self.containerView addSubview:self.TOSTextView];
  [self.containerView addSubview:self.optInLabel];
  [self.containerView addSubview:self.checkBoxButton];
  if ([self isBrowserManaged]) {
    [self.containerView addSubview:self.managedLabel];
    [self.containerView addSubview:self.enterpriseIcon];
  }
  [self addSubview:self.OKButton];
  [self configureSubviews];
}

- (void)safeAreaInsetsDidChange {
  [super safeAreaInsetsDidChange];
  [self layoutOKButtonAndContainerView];
}

- (void)layoutSubviews {
  // TODO(crbug.com/1157934): This page should support dynamic type to respect
  // the user's chosen font size. This layout might need to be changed for
  // smaller screen sizes and large fonts, as it might not fit a single screen,
  // especially with the "managed by organization" enterprise notice.
  [super layoutSubviews];
  [self layoutTitleLabel];
  [self layoutImageView];
  [self layoutTOSTextView];
  [self layoutOptInLabel];
  [self layoutCheckBoxButton];
  if ([self isBrowserManaged]) {
    [self layoutManagedLabel];
    [self layoutEnterpriseIcon];
  }
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
  // The image is centered and laid out below `titleLabel` as specified by
  // kImageTopPadding.
  CGSize imageViewSize = self.imageView.bounds.size;
  CGFloat imageViewTopPadding = kImageTopPadding[[self heightSizeClassIdiom]];
  self.imageView.frame = AlignRectOriginAndSizeToPixels(CGRectMake(
      (CGRectGetWidth(self.containerView.bounds) - imageViewSize.width) / 2.0,
      CGRectGetMaxY(self.titleLabel.frame) + imageViewTopPadding,
      imageViewSize.width, imageViewSize.height));
}

- (void)layoutTOSTextView {
  // The TOSTextView is centered and laid out below `imageView` as specified by
  // kTOSTextViewTopPadding.
  CGSize containerSize = self.containerView.bounds.size;
  containerSize.height = CGFLOAT_MAX;
  CGSize TOSTextViewSize = [self.TOSTextView sizeThatFits:containerSize];
  CGFloat TOSTextViewTopPadding =
      kTOSTextViewTopPadding[[self heightSizeClassIdiom]];
  CGRect frame =
      CGRectMake((containerSize.width - TOSTextViewSize.width) / 2.0,
                 CGRectGetMaxY(self.imageView.frame) + TOSTextViewTopPadding,
                 TOSTextViewSize.width, TOSTextViewSize.height);
  self.TOSTextView.frame = AlignRectOriginAndSizeToPixels(frame);
}

- (void)layoutOptInLabel {
  // The opt in label is laid out to the right (or left in RTL) of the check box
  // button and below `TOSLabel` as specified by kOptInLabelPadding.
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
                 CGRectGetMaxY(self.TOSTextView.frame) + optInLabelTopPadding,
                 optInLabelSize.width, optInLabelSize.height));
}

- (void)layoutCheckBoxButton {
  // The checkBoxButton is laid out to the left of `optInLabel`.  The view
  // itself is sized so that it covers the label, and the image insets are
  // chosen such that the check box image is centered vertically with
  // `optInLabel`.
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

- (void)layoutManagedLabel {
  // The managed label is laid out to the right (or left in RTL) of the
  // enterprise icon and below `optInLabel` as specified by
  // kManagedLabelPadding. It is aligned horizontally with `optInLabel`.
  CGSize checkBoxSize =
      [self.checkBoxButton imageForState:self.checkBoxButton.state].size;
  CGFloat checkBoxPadding = kCheckBoxPadding[[self widthSizeClassIdiom]];
  CGFloat managedLabelSidePadding = checkBoxSize.width + 2.0 * checkBoxPadding;
  CGSize managedLabelSize = [self.managedLabel
      sizeThatFits:CGSizeMake(CGRectGetWidth(self.optInLabel.bounds),
                              CGFLOAT_MAX)];
  CGFloat managedLabelTopPadding =
      kManagedLabelPadding[[self heightSizeClassIdiom]];
  CGFloat managedLabelOriginX =
      base::i18n::IsRTL()
          ? self.optInLabel.bounds.size.width - managedLabelSize.width
          : managedLabelSidePadding;

  self.managedLabel.frame = AlignRectOriginAndSizeToPixels(CGRectMake(
      managedLabelOriginX,
      CGRectGetMaxY(self.checkBoxButton.frame) + managedLabelTopPadding,
      managedLabelSize.width, managedLabelSize.height));
}

- (void)layoutEnterpriseIcon {
  // The enterprise icon is laid out to the left of and is centered vertically
  // with `managedLabel`, and is aligned horizontally with the checkbox image
  // inside `checkBoxButton`.
  CGSize enterpriseIconSize = self.enterpriseIcon.bounds.size;
  CGFloat enterpriseIconOriginX =
      CGRectGetMidX(self.checkBoxButton.imageView.frame) -
      enterpriseIconSize.height / 2.0;
  CGFloat enterpriseIconOriginY =
      CGRectGetMidY(self.managedLabel.frame) - enterpriseIconSize.height / 2.0;

  self.enterpriseIcon.frame = AlignRectOriginAndSizeToPixels(
      CGRectMake(enterpriseIconOriginX, enterpriseIconOriginY,
                 enterpriseIconSize.width, enterpriseIconSize.height));
}

- (void)layoutContainerView {
  // The container view is resized according to the final layout of its lowest
  // subview, which is `managedLabel` if the browser is managed and
  // `checkBoxButton` if not. The resized view is then centered horizontally and
  // vertically. If necessary, it is shifted up to allow either
  // `kmanagedLabelPadding` or `kOptInLabelPadding` (depending if the browser is
  // managed) between `optInLabel` and `OKButton`.
  CGSize containerViewSize = self.containerView.bounds.size;
  containerViewSize.height = [self isBrowserManaged]
                                 ? CGRectGetMaxY(self.managedLabel.frame)
                                 : CGRectGetMaxY(self.checkBoxButton.frame);

  CGFloat padding = [self isBrowserManaged]
                        ? kManagedLabelPadding[[self heightSizeClassIdiom]]
                        : kOptInLabelPadding[[self heightSizeClassIdiom]];

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
  [self configureTOSTextView];
  [self configureOptInLabel];
  if ([self isBrowserManaged]) {
    [self configureManagedLabel];
  }
  [self configureOKButton];
  [self setNeedsLayout];
}

- (void)configureTitleLabel {
  self.titleLabel.font =
      [UIFont systemFontOfSize:kTitleLabelFontSize[[self widthSizeClassIdiom]]
                        weight:UIFontWeightRegular];
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

- (void)configureTOSTextView {
  self.TOSTextView.scrollEnabled = NO;
  self.TOSTextView.editable = NO;
  self.TOSTextView.adjustsFontForContentSizeCategory = YES;
  self.TOSTextView.delegate = self;
  self.TOSTextView.backgroundColor = UIColor.clearColor;
  self.TOSTextView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};

  const StringWithTag parsedString = ParseStringWithTag(
      l10n_util::GetNSString(IDS_IOS_FIRSTRUN_AGREE_TO_TERMS), kBeginTOSLinkTag,
      kEndTOSLinkTag);
  DCHECK(parsedString.range != NSMakeRange(NSNotFound, 0));

  NSRange fullRange = NSMakeRange(0, parsedString.string.length);
  NSURL* URL =
      [NSURL URLWithString:base::SysUTF8ToNSString(kTermsOfServiceUrl)];
  UIFont* font = [UIFont
      systemFontOfSize:kTOSTOSTextViewFontSize[[self widthSizeClassIdiom]]
                weight:UIFontWeightRegular];
  NSMutableParagraphStyle* style =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  style.alignment = NSTextAlignmentCenter;

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string];
  [attributedText addAttributes:@{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSParagraphStyleAttributeName : style,
    NSFontAttributeName : font
  }
                          range:fullRange];
  [attributedText addAttribute:NSLinkAttributeName
                         value:URL
                         range:parsedString.range];

  self.TOSTextView.attributedText = attributedText;
}

- (void)configureOptInLabel {
  self.optInLabel.font =
      [UIFont systemFontOfSize:kOptInLabelFontSize[[self widthSizeClassIdiom]]
                        weight:UIFontWeightRegular];
  SetLabelLineHeight(self.optInLabel,
                     kOptInLabelLineHeight[[self widthSizeClassIdiom]]);
}

- (void)configureManagedLabel {
  self.managedLabel.font =
      [UIFont systemFontOfSize:kManagedLabelFontSize[[self widthSizeClassIdiom]]
                        weight:UIFontWeightRegular];
  self.managedLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  SetLabelLineHeight(self.managedLabel,
                     kManagedLabelLineHeight[[self widthSizeClassIdiom]]);
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
  UIFont* font = [UIFont
      systemFontOfSize:kOKButtonTitleLabelFontSize[[self widthSizeClassIdiom]]
                weight:UIFontWeightMedium];
  [self.OKButton setTitleFont:font forState:UIControlStateNormal];
  CGSize size = [self.OKButton
      sizeThatFits:CGSizeMake(CGFLOAT_MAX,
                              kOKButtonHeight[[self widthSizeClassIdiom]])];
  [self.OKButton
      setBounds:CGRectMake(0, 0, size.width,
                           kOKButtonHeight[[self widthSizeClassIdiom]])];
}

- (SizeClassIdiom)widthSizeClassIdiom {
  UIWindow* keyWindow = GetAnyKeyWindow();
  UIUserInterfaceSizeClass sizeClass = self.traitCollection.horizontalSizeClass;
  if (sizeClass == UIUserInterfaceSizeClassUnspecified)
    sizeClass = keyWindow.traitCollection.horizontalSizeClass;
  return GetSizeClassIdiom(sizeClass);
}

- (SizeClassIdiom)heightSizeClassIdiom {
  UIWindow* keyWindow = GetAnyKeyWindow();
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

#pragma mark - UITextViewDelegate

- (BOOL)textView:(TextViewSelectionDisabled*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(textView == self.TOSTextView);
  DCHECK(GURL(base::SysNSStringToUTF8(URL.absoluteString)) ==
         kTermsOfServiceUrl);
  [self.delegate welcomeToChromeViewDidTapTOSLink];
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

@end
