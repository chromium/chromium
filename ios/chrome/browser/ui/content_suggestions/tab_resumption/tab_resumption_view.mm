// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_view.h"

#import "base/check.h"
#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

namespace {

const CGFloat kImageContainerCornerRadius = 12.0;

// Image container with Salient image constants
const CGFloat kImageSalientContainerSize = 72.0;

// Image container without Salient image constants
const CGFloat kImageEmptyContainerSize = 56.0;

// Center Favicon constants.
const CGFloat kCenterFaviconSize = 24.0;
const CGFloat kCenterSymbolSize = 20.0;
const CGFloat kCenterFaviconCornerRadius = 5.0;
const CGFloat kCenterFaviconBackgroundSize = 30.0;
const CGFloat kCenterFaviconBackgroundCornerRadius = 7.0;

// Corner Favicon constants.
const CGFloat kCornerFaviconSize = 16.0;
const CGFloat kCornerFaviconCornerRadius = 4.0;
const CGFloat kCornerFaviconBackgroundSize = 24.0;
const CGFloat kCornerFaviconBackgroundCornerRadius = 4.0;
const CGFloat kCornerFaviconBackgroundBottomTrailingCornerRadius = 8.0;
const CGFloat kCornerFaviconSpace = 6.0;

// Stacks constants.
const CGFloat kContainerStackSpacing = 14.0;
const CGFloat kNewLabelStackSpacing = 6.0;
const CGFloat kLabelStackSpacing = 5.0;

// Adds the fallback image that should be used if there is no salient nor
// favicon image.
void SetFallbackImageToImageView(UIImageView* image_view,
                                 UIView* background_view,
                                 CGFloat favicon_size) {
  if (IsTabResumption1_5Enabled()) {
    image_view.image =
        DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kCenterSymbolSize);
    image_view.backgroundColor = [UIColor colorNamed:kBlue500Color];
    background_view.backgroundColor = [UIColor colorNamed:kBlue500Color];
    image_view.tintColor = UIColor.whiteColor;
  } else {
    image_view.image =
        CustomSymbolWithPointSize(kRecentTabsSymbol, kCenterSymbolSize);
    image_view.backgroundColor = [UIColor colorNamed:kGreen500Color];
    background_view.backgroundColor = [UIColor colorNamed:kGreen500Color];
    image_view.tintColor = UIColor.whiteColor;
  }
}

}  // namespace

@implementation TabResumptionView {
  // Item used to configure the view.
  TabResumptionItem* _item;
  // The view container.
  UIStackView* _containerStackView;
}

- (instancetype)initWithItem:(TabResumptionItem*)item {
  self = [super init];
  if (self) {
    _item = item;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  if (!_containerStackView) {
    [self createSubviews];
    [self addTapGestureRecognizer];
  }
}

#pragma mark - Private methods

// Creates all the subviews.
- (void)createSubviews {
  self.accessibilityIdentifier = kTabResumptionViewIdentifier;
  self.isAccessibilityElement = YES;
  self.accessibilityTraits = UIAccessibilityTraitButton;

  _containerStackView = [self configuredContainerStackView];

  UIView* imageContainerView = [self configuredImageContainer];
  [_containerStackView addArrangedSubview:imageContainerView];

  UIStackView* labelStackView = [self configuredLabelStackView];
  [_containerStackView addArrangedSubview:labelStackView];

  UILabel* sessionLabel;
  if (_item.itemType == TabResumptionItemType::kLastSyncedTab &&
      !IsTabResumption1_5Enabled()) {
    sessionLabel = [self configuredSessionLabel];
    [labelStackView addArrangedSubview:sessionLabel];
  }
  UILabel* tabTitleLabel = [self configuredTabTitleLabel];
  [labelStackView addArrangedSubview:tabTitleLabel];
  UILabel* hostnameAndSyncTimeLabel = [self configuredHostNameAndSyncTimeLabel];
  [labelStackView addArrangedSubview:hostnameAndSyncTimeLabel];

  if (_item.itemType == TabResumptionItemType::kLastSyncedTab) {
    self.accessibilityLabel = [NSString
        stringWithFormat:@"%@,%@,%@", sessionLabel.text, tabTitleLabel.text,
                         hostnameAndSyncTimeLabel.text];
  } else {
    self.accessibilityLabel =
        [NSString stringWithFormat:@"%@,%@", tabTitleLabel.text,
                                   hostnameAndSyncTimeLabel.text];
  }

  [self addSubview:_containerStackView];
  AddSameConstraints(_containerStackView, self);
}

// Adds a tap gesture recognizer to the view.
- (void)addTapGestureRecognizer {
  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(tabResumptionItemTapped:)];
  [self addGestureRecognizer:tapRecognizer];
}

// Configures and returns the UIStackView that contains the faviconContainerView
// and the labelsStackView.
- (UIStackView*)configuredContainerStackView {
  UIStackView* containerStack = [[UIStackView alloc] init];
  containerStack.axis = UILayoutConstraintAxisHorizontal;
  containerStack.spacing = kContainerStackSpacing;
  containerStack.distribution = UIStackViewDistributionFill;
  containerStack.alignment = UIStackViewAlignmentCenter;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  return containerStack;
}

// Configures and returns the UIStackView that contains the different labels.
- (UIStackView*)configuredLabelStackView {
  UIStackView* labelStackView = [[UIStackView alloc] init];
  labelStackView.axis = UILayoutConstraintAxisVertical;
  if (IsTabResumption1_5Enabled()) {
    labelStackView.spacing = kNewLabelStackSpacing;
  } else {
    labelStackView.spacing = kLabelStackSpacing;
  }
  labelStackView.translatesAutoresizingMaskIntoConstraints = NO;
  return labelStackView;
}

// Configures and returns the leading UIView that may contain the favicon image.
- (UIView*)configuredFaviconViewWithSalientImage:(BOOL)hasSalientImage {
  UIView* faviconBackgroundView = [[UIView alloc] init];
  faviconBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconBackgroundView.backgroundColor = UIColor.whiteColor;
  UIImageView* faviconImageView = [[UIImageView alloc] init];

  CGFloat faviconSize;
  CGFloat faviconCornerRadius;
  CGFloat faviconBackgoundSize;
  CGFloat faviconBackgroundCornerRadius;
  if (hasSalientImage) {
    faviconSize = kCornerFaviconSize;
    faviconCornerRadius = kCornerFaviconCornerRadius;
    faviconBackgoundSize = kCornerFaviconBackgroundSize;
    faviconBackgroundCornerRadius = kCornerFaviconBackgroundCornerRadius;
  } else {
    faviconSize = kCenterFaviconSize;
    faviconCornerRadius = kCenterFaviconCornerRadius;
    faviconBackgoundSize = kCenterFaviconBackgroundSize;
    faviconBackgroundCornerRadius = kCenterFaviconBackgroundCornerRadius;
  }

  if (_item.faviconImage) {
    faviconImageView.image = _item.faviconImage;
  } else {
    SetFallbackImageToImageView(faviconImageView, faviconBackgroundView,
                                faviconSize);
  }

  faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconImageView.contentMode = UIViewContentModeScaleAspectFit;
  faviconImageView.clipsToBounds = YES;
  [faviconBackgroundView addSubview:faviconImageView];

  faviconBackgroundView.layer.cornerRadius = faviconBackgroundCornerRadius;
  faviconImageView.layer.cornerRadius = faviconCornerRadius;
  [NSLayoutConstraint activateConstraints:@[
    [faviconBackgroundView.widthAnchor
        constraintEqualToConstant:faviconBackgoundSize],
    [faviconBackgroundView.heightAnchor
        constraintEqualToConstant:faviconBackgoundSize],
    [faviconImageView.widthAnchor constraintEqualToConstant:faviconSize],
    [faviconImageView.heightAnchor constraintEqualToConstant:faviconSize],
  ]];
  AddSameCenterConstraints(faviconBackgroundView, faviconImageView);

  if (hasSalientImage) {
    UIRectCorner bottomTrail = UIRectCornerBottomRight;
    if (base::i18n::IsRTL()) {
      bottomTrail = UIRectCornerBottomLeft;
    }

    CGSize cornerRadiusSize =
        CGSizeMake(kCornerFaviconBackgroundBottomTrailingCornerRadius,
                   kCornerFaviconBackgroundBottomTrailingCornerRadius);
    UIBezierPath* bezierPath = [UIBezierPath
        bezierPathWithRoundedRect:CGRectMake(0, 0, faviconBackgoundSize,
                                             faviconBackgoundSize)
                byRoundingCorners:bottomTrail
                      cornerRadii:cornerRadiusSize];
    CAShapeLayer* mask = [CAShapeLayer layer];
    mask.path = bezierPath.CGPath;
    faviconBackgroundView.layer.mask = mask;
  }
  return faviconBackgroundView;
}

// Configures and returns the leading UIView that may contain the Salient image.
- (UIView*)configuredSalientImageViewWithSize:(CGFloat)containerSize {
  UIImageView* salientView = [[UIImageView alloc] init];

  // Compute the size of the image.
  CGFloat width = _item.contentImage.size.width;
  CGFloat height = _item.contentImage.size.height;
  if (width > height) {
    width = (width * containerSize) / height;
    height = containerSize;
  } else {
    height = (height * containerSize) / width;
    width = containerSize;
  }

  // Resize the salient image.
  UIGraphicsBeginImageContextWithOptions(CGSize(width, height), NO, 0.0);
  [_item.contentImage drawInRect:CGRectMake(0, 0, width, height)];
  UIImage* scaledImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  [salientView setImage:scaledImage];

  salientView.translatesAutoresizingMaskIntoConstraints = NO;

  salientView.contentMode = UIViewContentModeTop;

  // Add a gradient overlay.
  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.frame = CGRectMake(0, 0, containerSize, containerSize);
  gradientLayer.startPoint = CGPointMake(0.0, 0.0);
  gradientLayer.endPoint = CGPointMake(0.0, 1.0);
  gradientLayer.colors = @[
    static_cast<id>([UIColor clearColor].CGColor),
    static_cast<id>([UIColor colorWithWhite:0 alpha:0.2].CGColor)
  ];
  [salientView.layer insertSublayer:gradientLayer atIndex:0];
  return salientView;
}

// Configures and returns the leading UIView that contains the image.
- (UIView*)configuredImageContainer {
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.layer.cornerRadius = kImageContainerCornerRadius;
  containerView.clipsToBounds = YES;

  BOOL hasSalientImage = NO;
  CGFloat containerSize;
  if (_item.contentImage && IsTabResumption1_5SalientImageEnabled() &&
      _item.contentImage.size.width && _item.contentImage.size.height) {
    hasSalientImage = YES;
    containerSize = kImageSalientContainerSize;
    UIView* salientView =
        [self configuredSalientImageViewWithSize:containerSize];
    [containerView addSubview:salientView];
    AddSameConstraints(salientView, containerView);
  } else {
    containerView.backgroundColor =
        [UIColor colorNamed:kTertiaryBackgroundColor];
    containerSize = kImageEmptyContainerSize;
  }

  [NSLayoutConstraint activateConstraints:@[
    [containerView.widthAnchor constraintEqualToConstant:containerSize],
    [containerView.heightAnchor constraintEqualToConstant:containerSize],
  ]];

  if (hasSalientImage && !_item.faviconImage) {
    return containerView;
  }

  UIView* faviconView =
      [self configuredFaviconViewWithSalientImage:hasSalientImage];
  [containerView addSubview:faviconView];
  if (!hasSalientImage) {
    AddSameCenterConstraints(faviconView, containerView);
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [faviconView.trailingAnchor
          constraintEqualToAnchor:containerView.trailingAnchor
                         constant:-kCornerFaviconSpace],
      [faviconView.bottomAnchor
          constraintEqualToAnchor:containerView.bottomAnchor
                         constant:-kCornerFaviconSpace],
    ]];
  }
  return containerView;
}

// Configures and returns the UILabel that contains the session name.
- (UILabel*)configuredSessionLabel {
  NSString* sessionString =
      l10n_util::GetNSStringF(IDS_IOS_TAB_RESUMPTION_TILE_HOST_LABEL,
                              base::SysNSStringToUTF16(_item.sessionName));

  UILabel* label = [[UILabel alloc] init];
  label.text = sessionString.uppercaseString;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  label.numberOfLines = 1;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

// Configures and returns the UILabel that contains the session name.
- (UILabel*)configuredTabTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.text = [_item.tabTitle length]
                   ? _item.tabTitle
                   : l10n_util::GetNSString(
                         IDS_IOS_TAB_RESUMPTION_TAB_TITLE_PLACEHOLDER);
  label.font = CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold);
  label.numberOfLines = IsTabResumption1_5Enabled() ? 2 : 1;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];

  return label;
}

// Configures and returns the UILabel that contains the session name.
- (UILabel*)configuredHostNameAndSyncTimeLabel {
  NSString* hostnameAndSyncTimeString;
  UILabel* label = [[UILabel alloc] init];
  if (IsTabResumption1_5Enabled()) {
    if (_item.itemType == kLastSyncedTab && _item.sessionName) {
      hostnameAndSyncTimeString = [NSString
          stringWithFormat:@"%@ • %@", [self hostnameFromGURL:_item.tabURL],
                           _item.sessionName];
    } else {
      hostnameAndSyncTimeString = [NSString
          stringWithFormat:@"%@", [self hostnameFromGURL:_item.tabURL]];
    }
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  } else {
    hostnameAndSyncTimeString = [NSString
        stringWithFormat:@"%@ • %@", [self hostnameFromGURL:_item.tabURL],
                         [self lastSyncTimeStringFromTime:_item.syncedTime]];
    label.lineBreakMode = NSLineBreakByTruncatingMiddle;
    label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  }

  label.text = hostnameAndSyncTimeString;

  label.numberOfLines = 1;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

// Returns the tab hostname from the given `URL`.
- (NSString*)hostnameFromGURL:(GURL)URL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
}

// Returns the last sync string from the given `time`.
- (NSString*)lastSyncTimeStringFromTime:(base::Time)time {
  base::TimeDelta lastUsedDelta = base::Time::Now() - time;
  base::TimeDelta oneMinuteDelta = base::Minutes(1);

  // If the tab was synchronized within the last minute, returns 'just now'.
  if (lastUsedDelta < oneMinuteDelta) {
    return l10n_util::GetNSString(IDS_IOS_TAB_RESUMPTION_JUST_NOW);
  }

  // This will return something similar to "2 mins/hours ago".
  return base::SysUTF16ToNSString(
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                             ui::TimeFormat::LENGTH_SHORT, lastUsedDelta));
}

// Called when the view has been tapped.
- (void)tabResumptionItemTapped:(UIGestureRecognizer*)sender {
  [self.commandHandler openTabResumptionItem:_item];
}

@end
