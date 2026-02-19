// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_view.h"

#import "base/check.h"
#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/public/tab_resumption_constants.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_commands.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_config.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/price_notifications/ui_bundled/cells/price_notifications_price_chip_view.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

namespace {

const CGFloat kImageContainerCornerRadius = 12.0;

// Image container with Content image constants
const CGFloat kImageContentContainerSize = 72.0;

// Image container without Content image constants
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
const CGFloat kLabelStackSpacing = 6.0;

// Title constants.
const CGFloat kTitleLineSpacing = 18.0;

// Alpha for start/end gradient for product image overlay.
const CGFloat kPriceDropOverlayStartAlpha = 0.0;
const CGFloat kPriceDropOverlayEndAlpha = 0.14;

// Adds the fallback image that should be used if there is no content nor
// favicon image.
void SetFallbackImageToImageView(UIImageView* image_view,
                                 UIView* background_view,
                                 CGFloat favicon_size) {
  image_view.image =
      DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kCenterSymbolSize);
  image_view.backgroundColor = [UIColor colorNamed:kBlue500Color];
  background_view.backgroundColor = [UIColor colorNamed:kBlue500Color];
  image_view.tintColor = UIColor.whiteColor;
}

bool HasPriceDropOnTab(TabResumptionConfig* config) {
  return config.shopCardData &&
         config.shopCardData.shopCardItemType ==
             ShopCardItemType::kPriceDropOnTab &&
         config.shopCardData.priceDrop.has_value();
}

}  // namespace

@implementation TabResumptionView {
  // Item used to configure the view.
  TabResumptionConfig* _config;
  // The view container.
  UIStackView* _containerStackView;

  // Displays the price drop chip if a price drop exists for
  // the tab resumption url.
  PriceNotificationsPriceChipView* _priceNotificationsChip;

  // Image representing the tab.
  UIView* _imageContainerView;

  // Title of the Tab.
  UILabel* _tabTitleLabel;

  // Containts Tab title, host name and price
  // drop chip if applicable.
  UIStackView* _labelStackView;
}

- (instancetype)initWithConfig:(TabResumptionConfig*)config {
  if ((self = [super init])) {
    _config = config;
    if (IsNTPBackgroundCustomizationEnabled()) {
      [self registerForTraitChanges:@[ NewTabPageTrait.class ]
                         withAction:@selector(applyBackgroundColors)];
    }
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
  [self clearSubviews];
  self.accessibilityIdentifier = kTabResumptionViewIdentifier;
  self.isAccessibilityElement = YES;
  self.accessibilityTraits = UIAccessibilityTraitButton;

  _containerStackView = [self configuredContainerStackView];

  _imageContainerView = [self configuredImageContainer];
  [_containerStackView addArrangedSubview:_imageContainerView];

  _labelStackView = [self configuredLabelStackView];
  [_containerStackView addArrangedSubview:_labelStackView];

  _tabTitleLabel = [self configuredTabTitleLabel];
  [_labelStackView addArrangedSubview:_tabTitleLabel];
  NSMutableArray* accessibilityLabel = [NSMutableArray array];
  UILabel* hostnameAndSyncTimeLabel = [self configuredHostNameAndSyncTimeLabel];
  [_labelStackView addArrangedSubview:hostnameAndSyncTimeLabel];
  [accessibilityLabel addObject:hostnameAndSyncTimeLabel.text];

  if (HasPriceDropOnTab(_config)) {
    _tabTitleLabel.numberOfLines = 1;
    _priceNotificationsChip = [[PriceNotificationsPriceChipView alloc] init];
    _priceNotificationsChip.translatesAutoresizingMaskIntoConstraints = NO;
    _priceNotificationsChip.isAccessibilityElement = YES;
    _priceNotificationsChip.previousPriceFont =
        PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightMedium);
    _priceNotificationsChip.currentPriceFont =
        PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightMedium);
    _priceNotificationsChip.strikeoutPreviousPrice = YES;
    [_priceNotificationsChip
         setPriceDrop:_config.shopCardData.priceDrop->current_price
        previousPrice:_config.shopCardData.priceDrop->previous_price];
    [_labelStackView addArrangedSubview:_priceNotificationsChip];
    self.accessibilityLabel = _config.shopCardData.accessibilityString;
    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.self ]
                       withAction:@selector(hidePriceDropOnTraitChange)];

  } else {
    self.accessibilityLabel =
        [accessibilityLabel componentsJoinedByString:@", "];
  }

  [self addSubview:_containerStackView];
  AddSameConstraints(_containerStackView, self);

  [self applyBackgroundColors];
}

- (void)clearSubviews {
  if (_priceNotificationsChip) {
    [_priceNotificationsChip removeFromSuperview];
  }
  if (_imageContainerView) {
    [_imageContainerView removeFromSuperview];
  }
  if (_tabTitleLabel) {
    [_tabTitleLabel removeFromSuperview];
  }

  if (_labelStackView) {
    [_labelStackView removeFromSuperview];
  }
  if (_containerStackView) {
    [_containerStackView removeFromSuperview];
  }
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
  labelStackView.alignment = UIStackViewAlignmentLeading;
  labelStackView.spacing = kLabelStackSpacing;

  labelStackView.translatesAutoresizingMaskIntoConstraints = NO;
  return labelStackView;
}

// Configures and returns the leading UIView that may contain the favicon image.
- (UIView*)configuredFaviconViewWithContentImage:(BOOL)hasContentImage {
  UIView* faviconBackgroundView = [[UIView alloc] init];
  faviconBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconBackgroundView.backgroundColor = UIColor.whiteColor;
  UIImageView* faviconImageView = [[UIImageView alloc] init];

  CGFloat faviconSize;
  CGFloat faviconCornerRadius;
  CGFloat faviconBackgoundSize;
  CGFloat faviconBackgroundCornerRadius;
  if (hasContentImage) {
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

  if (_config.faviconImage) {
    faviconImageView.image = _config.faviconImage;
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

  if (hasContentImage) {
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

// Configures and returns the leading UIView that may contain the Content image.
- (UIView*)configuredContentImageViewWithSize:(CGFloat)containerSize {
  UIImageView* contentImageView = [[UIImageView alloc] init];

  // Compute the size of the image.
  CGFloat width = _config.contentImage.size.width;
  CGFloat height = _config.contentImage.size.height;
  if (width > height) {
    width = (width * containerSize) / height;
    height = containerSize;
  } else {
    height = (height * containerSize) / width;
    width = containerSize;
  }

  // Resize the content image.
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = 0.0;
  format.opaque = NO;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:CGSize(width, height)
                                             format:format];
  UIImage* scaledImage =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [_config.contentImage drawInRect:CGRectMake(0, 0, width, height)];
      }];
  [contentImageView setImage:scaledImage];

  contentImageView.translatesAutoresizingMaskIntoConstraints = NO;

  contentImageView.contentMode = UIViewContentModeTop;

  // Add a gradient overlay.
  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.frame = CGRectMake(0, 0, containerSize, containerSize);
  gradientLayer.startPoint = CGPointMake(0.0, 0.0);
  gradientLayer.endPoint = CGPointMake(0.0, 1.0);
  if (HasPriceDropOnTab(_config)) {
    gradientLayer.colors = @[
      static_cast<id>([[UIColor blackColor]
                          colorWithAlphaComponent:kPriceDropOverlayStartAlpha]
                          .CGColor),
      static_cast<id>([[UIColor blackColor]
                          colorWithAlphaComponent:kPriceDropOverlayEndAlpha]
                          .CGColor)
    ];
  } else {
    gradientLayer.colors = @[
      static_cast<id>([UIColor clearColor].CGColor),
      static_cast<id>([UIColor colorWithWhite:0 alpha:0.2].CGColor)
    ];
  }
  [contentImageView.layer insertSublayer:gradientLayer atIndex:0];
  return contentImageView;
}

// Configures and returns the leading UIView that contains the image.
- (UIView*)configuredImageContainer {
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.layer.cornerRadius = kImageContainerCornerRadius;
  containerView.clipsToBounds = YES;

  BOOL hasContentImage = NO;
  CGFloat containerSize;
  if (_config.contentImage && _config.contentImage.size.width &&
      _config.contentImage.size.height) {
    hasContentImage = YES;
    containerSize = kImageContentContainerSize;
    UIView* contentImageView =
        [self configuredContentImageViewWithSize:containerSize];
    [containerView addSubview:contentImageView];
    AddSameConstraints(contentImageView, containerView);
  } else {
    containerSize = kImageEmptyContainerSize;
  }

  [NSLayoutConstraint activateConstraints:@[
    [containerView.widthAnchor constraintEqualToConstant:containerSize],
    [containerView.heightAnchor constraintEqualToConstant:containerSize],
  ]];

  if (hasContentImage && !_config.faviconImage) {
    return containerView;
  }

  UIView* faviconView =
      [self configuredFaviconViewWithContentImage:hasContentImage];
  [containerView addSubview:faviconView];
  if (!hasContentImage) {
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
                              base::SysNSStringToUTF16(_config.sessionName));

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
  NSString* text = [_config.tabTitle length]
                       ? _config.tabTitle
                       : l10n_util::GetNSString(
                             IDS_IOS_TAB_RESUMPTION_TAB_TITLE_PLACEHOLDER);
  UIFont* font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightSemibold);
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  style.lineHeightMultiple = kTitleLineSpacing / font.lineHeight;
  style.lineBreakMode = NSLineBreakByTruncatingTail;
  NSAttributedString* attrString = [[NSAttributedString alloc]
      initWithString:text
          attributes:@{
            NSFontAttributeName : font,
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextPrimaryColor],
            NSParagraphStyleAttributeName : style
          }];
  label.attributedText = attrString;
  label.numberOfLines = 2;
  label.adjustsFontForContentSizeCategory = YES;

  return label;
}

// Configures and returns the UILabel that contains the session name.
- (UILabel*)configuredHostNameAndSyncTimeLabel {
  NSString* hostnameAndSyncTimeString;
  UILabel* label = [[UILabel alloc] init];
  if (_config.itemType == kLastSyncedTab && _config.sessionName) {
    hostnameAndSyncTimeString = [NSString
        stringWithFormat:@"%@ • %@", [self hostnameFromGURL:_config.tabURL],
                         _config.sessionName];
  } else {
    hostnameAndSyncTimeString = [NSString
        stringWithFormat:@"%@ • %@", [self hostnameFromGURL:_config.tabURL],
                         [self lastSyncTimeStringFromTime:_config.syncedTime]];
  }
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];

  label.text = hostnameAndSyncTimeString;

  label.numberOfLines = 1;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

- (void)applyBackgroundColors {
  // Only set background color if item does not have content image.
  BOOL hasContentImage = _config.contentImage &&
                         _config.contentImage.size.width &&
                         _config.contentImage.size.height;
  if (!hasContentImage) {
    NewTabPageColorPalette* colorPalette =
        [self.traitCollection objectForNewTabPageTrait];
    _imageContainerView.backgroundColor =
        colorPalette.tertiaryColor ?: [UIColor colorNamed:kGrey100Color];
  }
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
  [self.tabResumptionHandler openTabResumptionItem:_config];
}

- (void)hidePriceDropOnTraitChange {
  _priceNotificationsChip.hidden =
      self.traitCollection.preferredContentSizeCategory >
      UIContentSizeCategoryExtraLarge;
}

@end
