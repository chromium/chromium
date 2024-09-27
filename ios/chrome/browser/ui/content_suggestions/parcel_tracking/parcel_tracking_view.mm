// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_view.h"

#import "base/i18n/time_formatting.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Spacing between Icon container and the text StackView.
const CGFloat kIconContainerTextSpacing = 20.0f;

// Corner radius of the Icon container.
const CGFloat kIconContainerCornerRadius = 12.0f;

// Icon container size.
const CGFloat kIconContainerWidth = 72.0f;

// Margin between icon and its container when using the default image.
const CGFloat kIconContainerMargin = 18.0f;

// Margin between icon and its container when using the carrier's logo.
const CGFloat kIconContainerMarginForCarrierLogo = 8.0f;

// Size of the icon.
const CGFloat kIconSize = 53.0f;

// Spacing between text StackView subviews.
const CGFloat kTextStackViewSpacing = 5.0f;

// Spacing between status bars.
const CGFloat kStatusBarViewSpacing = 6.0f;

// Status bar configurations.
const CGFloat kStatusBarWidth = 61.0f;
const CGFloat kStatusBarHeight = 6.0f;
const CGFloat kStatusBarCornerRadius = 3.0f;
const CGFloat kStatusBarBottomMarginViewHeight = .01f;

BOOL isInProgressState(ParcelState state) {
  return state == ParcelState::kPickedUp || state == ParcelState::kHandedOff ||
         state == ParcelState::kWithCarrier;
}

}  // namespace

// Represents a status bar in the ParcelStatusBarView.
@interface ParcelStatusBarView : UIView

// Configures the view to reflect `hasError` and if it should have `lighterTone`
// coloring.
- (void)configureAsError:(BOOL)hasError lighterTone:(BOOL)lighterTone;

@end

@implementation ParcelStatusBarView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.cornerRadius = kStatusBarCornerRadius;
    self.layer.masksToBounds = YES;

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintEqualToConstant:kStatusBarWidth],
      [self.heightAnchor constraintEqualToConstant:kStatusBarHeight],
    ]];

    [self setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisVertical];
    [self
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];
  }
  return self;
}

- (void)configureAsError:(BOOL)hasError lighterTone:(BOOL)lighterTone {
  NSString* colorName;
  if (hasError) {
    colorName = lighterTone ? kRed100Color : kRed400Color;
  } else {
    colorName = lighterTone ? kGreen100Color : kGreen400Color;
  }
  self.backgroundColor = [UIColor colorNamed:colorName];
}

@end

@implementation ParcelTrackingModuleView {
  UIView* _imageContainer;
  ParcelStatusBarView* _firstStatusBar;
  ParcelStatusBarView* _secondStatusBar;
  ParcelStatusBarView* _thirdStatusBar;
  GURL _parcelTrackingURL;
  UIImageView* _iconImageView;
  UILabel* _titleLabel;
  UILabel* _subtitleLabel;
  UITapGestureRecognizer* _tapGestureRecognizer;
  BOOL _useCarrierLogo;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self constructView];
    self.isAccessibilityElement = YES;
    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
        UITraitUserInterfaceStyle.self, UITraitPreferredContentSizeCategory.self
      ]);
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf updateUIOnTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

- (void)configureView:(ParcelTrackingItem*)config {
  _parcelTrackingURL = config.trackingURL;
  _iconImageView.image = [self iconImageForParcelType:config.parcelType];
  _imageContainer.layer.borderWidth = [self iconBorderWidth];

  NSString* carrierName;
  switch (config.parcelType) {
    case ParcelType::kUSPS:
      carrierName =
          l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CARRIER_USPS);
      break;
    case ParcelType::kUPS:
      carrierName = l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CARRIER_UPS);
      break;
    case ParcelType::kFedex:
      carrierName =
          l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CARRIER_FEDEX);
      break;
    default:
      break;
  }
  _subtitleLabel.text = l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_INFORMATION,
      base::SysNSStringToUTF16(carrierName),
      base::SysNSStringToUTF16(config.parcelID));

  [self updateViewForParcelStatus:config.status
                     deliveryTime:config.estimatedDeliveryTime];

  self.accessibilityLabel = [NSString
      stringWithFormat:@"%@, %@", _titleLabel.text, _subtitleLabel.text];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange:previousTraitCollection];
}
#endif

- (void)constructView {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.isAccessibilityElement = NO;
  _titleLabel.font =
      CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold);
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.numberOfLines = 0;
  _titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _titleLabel.textColor = [UIColor colorNamed:kGreen600Color];
  [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultLow
                                 forAxis:UILayoutConstraintAxisVertical];
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  _subtitleLabel = [[UILabel alloc] init];
  _subtitleLabel.numberOfLines = 1;
  _subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _subtitleLabel.adjustsFontForContentSizeCategory = YES;
  _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

  _firstStatusBar = [[ParcelStatusBarView alloc] init];
  _secondStatusBar = [[ParcelStatusBarView alloc] init];
  _thirdStatusBar = [[ParcelStatusBarView alloc] init];
  UIStackView* statusBarStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _firstStatusBar, _secondStatusBar, _thirdStatusBar
      ]];
  statusBarStackView.axis = UILayoutConstraintAxisHorizontal;
  statusBarStackView.alignment = UIStackViewAlignmentCenter;
  statusBarStackView.spacing = kStatusBarViewSpacing;

  // Add empty view to serve as spacing between status bars and subtitle that
  // dynamically expands vertically to fill space.
  UIView* emptySpaceFiller = [[UIView alloc] init];
  [emptySpaceFiller setContentHuggingPriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  // Add empty view to trigger spacing between status bars and bottom alignment
  // with the image.
  UIView* statusBarBottomMarginView = [[UIView alloc] init];
  [NSLayoutConstraint activateConstraints:@[
    [statusBarBottomMarginView.heightAnchor
        constraintEqualToConstant:kStatusBarBottomMarginViewHeight]
  ]];
  UIStackView* rightVerticalStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _titleLabel, _subtitleLabel, emptySpaceFiller, statusBarStackView,
        statusBarBottomMarginView
      ]];
  rightVerticalStackView.axis = UILayoutConstraintAxisVertical;
  rightVerticalStackView.alignment = UIStackViewAlignmentLeading;
  rightVerticalStackView.spacing = kTextStackViewSpacing;

  _iconImageView = [[UIImageView alloc] init];
  _iconImageView.contentMode = UIViewContentModeScaleAspectFit;
  _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;

  // Container allows for margins between icon a border.
  _imageContainer = [[UIView alloc] init];
  _imageContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _imageContainer.layer.cornerRadius = kIconContainerCornerRadius;
  _imageContainer.layer.masksToBounds = YES;
  _imageContainer.layer.borderColor =
      [UIColor colorNamed:kGrey200Color].CGColor;
  [_imageContainer addSubview:_iconImageView];
  CGFloat containerMargin = _useCarrierLogo ? kIconContainerMarginForCarrierLogo
                                            : kIconContainerMargin;
  AddSameConstraintsWithInsets(
      _iconImageView, _imageContainer,
      NSDirectionalEdgeInsetsMake(containerMargin, containerMargin,
                                  containerMargin, containerMargin));

  UIStackView* horizontalStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _imageContainer, rightVerticalStackView ]];
  horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  horizontalStackView.alignment = UIStackViewAlignmentTrailing;
  horizontalStackView.spacing = kIconContainerTextSpacing;
  [self addSubview:horizontalStackView];
  AddSameConstraints(horizontalStackView, self);

  [NSLayoutConstraint activateConstraints:@[
    [_imageContainer.widthAnchor constraintEqualToConstant:kIconContainerWidth],
    [_imageContainer.heightAnchor
        constraintEqualToAnchor:_imageContainer.widthAnchor],
    [rightVerticalStackView.topAnchor
        constraintLessThanOrEqualToAnchor:_imageContainer.topAnchor],
  ]];

  // Set up the tap gesture recognizer.
  _tapGestureRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];
  [self addGestureRecognizer:_tapGestureRecognizer];
}

// Returns the appropriate icon image for a `parcelType`.
- (UIImage*)iconImageForParcelType:(ParcelType)parcelType {
#if !BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  _useCarrierLogo = NO;
    return DefaultSymbolWithPointSize(kBoxTruckFillSymbol, kIconSize);
#else
  switch (parcelType) {
    case ParcelType::kUPS:
      _useCarrierLogo = YES;
      return [UIImage imageNamed:kUPSCarrierImage];
    case ParcelType::kFedex:
      _useCarrierLogo = YES;
      return [UIImage imageNamed:kFedexCarrierImage];
    default:
      _useCarrierLogo = NO;
        return DefaultSymbolWithPointSize(kBoxTruckFillSymbol, kIconSize);
  }
#endif
}

// Updates the title and status bars based on the parcel `status` and
// `estimatedDeliveryTime`.
- (void)updateViewForParcelStatus:(ParcelState)status
                     deliveryTime:
                         (std::optional<base::Time>)estimatedDeliveryTime {
  NSString* dateString =
      estimatedDeliveryTime.has_value()
          ? base::SysUTF16ToNSString(base::LocalizedTimeFormatWithPattern(
                *estimatedDeliveryTime, "EEEE MMMM d"))
          : nil;
  NSString* imageColorName;
  NSString* imageContainerColorName;

  // If the parcel is in progress but the estimated delivered date was not set
  // on the server side, we cannot show the user much useful information. As
  // such, we treat those cases visually like they are in the new parcel state.
  if (!estimatedDeliveryTime.has_value() && isInProgressState(status)) {
    status = ParcelState::kNew;
  }

  // Configure the status bars (and title text color if needed) depending on
  // status.
  switch (status) {
    case ParcelState::kNew:
      _titleLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_NEW_STATUS);
      [_firstStatusBar configureAsError:NO lighterTone:NO];
      [_secondStatusBar configureAsError:NO lighterTone:YES];
      [_thirdStatusBar configureAsError:NO lighterTone:YES];
      imageColorName = kGreen300Color;
      imageContainerColorName = kStaticGreen50Color;
      break;
    case ParcelState::kLabelCreated:
      _titleLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_LABEL_CREATED_STATUS);
      [_firstStatusBar configureAsError:NO lighterTone:NO];
      [_secondStatusBar configureAsError:NO lighterTone:YES];
      [_thirdStatusBar configureAsError:NO lighterTone:YES];
      imageColorName = kGreen300Color;
      imageContainerColorName = kStaticGreen50Color;
      break;
    case ParcelState::kFinished: {
      if (!estimatedDeliveryTime.has_value()) {
        _titleLabel.text = l10n_util::GetNSString(
            IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_DELIVERED_STATUS);
      } else {
        // Use Today date descriptor if the delivery day matches the current day
        if ([[NSCalendar currentCalendar]
                         isDate:estimatedDeliveryTime->ToNSDate()
                inSameDayAsDate:[NSDate date]]) {
          dateString = l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_DELIVERED_TODAY);
        }
        _titleLabel.text = [NSString
            stringWithFormat:
                @"%@ %@",
                l10n_util::GetNSString(
                    IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_DELIVERED_STATUS),
                dateString];
      }
      [_firstStatusBar configureAsError:NO lighterTone:NO];
      [_secondStatusBar configureAsError:NO lighterTone:NO];
      [_thirdStatusBar configureAsError:NO lighterTone:NO];
      imageColorName = kGreen300Color;
      imageContainerColorName = kStaticGreen50Color;
      break;
    }
    case ParcelState::kAtPickupLocation:
      _titleLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_READY_PICKUP_STATUS);
      [_firstStatusBar configureAsError:NO lighterTone:NO];
      [_secondStatusBar configureAsError:NO lighterTone:NO];
      [_thirdStatusBar configureAsError:NO lighterTone:NO];
      imageColorName = kGreen300Color;
      imageContainerColorName = kStaticGreen50Color;
      break;
    case ParcelState::kPickedUp:
    case ParcelState::kHandedOff:
    case ParcelState::kWithCarrier:
      _titleLabel.text = l10n_util::GetNSStringF(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_ARRIVING_STATUS,
          base::SysNSStringToUTF16(dateString));
      [_firstStatusBar configureAsError:NO lighterTone:NO];
      [_secondStatusBar configureAsError:NO lighterTone:NO];
      [_thirdStatusBar configureAsError:NO lighterTone:YES];
      imageColorName = kGreen300Color;
      imageContainerColorName = kStaticGreen50Color;
      break;
    case ParcelState::kOutForDelivery:
      _titleLabel.text = l10n_util::GetNSStringF(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_ARRIVING_STATUS,
          base::SysNSStringToUTF16(l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_DELIVERED_TODAY)));
      [_firstStatusBar configureAsError:NO lighterTone:NO];
      [_secondStatusBar configureAsError:NO lighterTone:NO];
      [_thirdStatusBar configureAsError:NO lighterTone:YES];
      imageColorName = kGreen300Color;
      imageContainerColorName = kStaticGreen50Color;
      break;
    case ParcelState::kDeliveryFailed:
      _titleLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_DELIVERY_ATTEMPTED_STATUS);
      _titleLabel.textColor = [UIColor colorNamed:kRed600Color];
      [_firstStatusBar configureAsError:YES lighterTone:NO];
      [_secondStatusBar configureAsError:YES lighterTone:NO];
      [_thirdStatusBar configureAsError:YES lighterTone:YES];
      imageColorName = kRed300Color;
      imageContainerColorName = kRed50Color;
      break;
    case ParcelState::kError:
      _titleLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_ERROR_STATUS);
      _titleLabel.textColor = [UIColor colorNamed:kRed600Color];
      [_firstStatusBar configureAsError:YES lighterTone:NO];
      [_secondStatusBar configureAsError:YES lighterTone:NO];
      [_thirdStatusBar configureAsError:YES lighterTone:YES];
      imageColorName = kRed300Color;
      imageContainerColorName = kRed50Color;
      break;
    case ParcelState::kCancelled:
      _titleLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_CANCELLED_STATUS);
      _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
      // No status bars.
      [_firstStatusBar removeFromSuperview];
      [_secondStatusBar removeFromSuperview];
      [_thirdStatusBar removeFromSuperview];
      imageColorName = kGrey400Color;
      imageContainerColorName = kGrey100Color;
      break;
    case ParcelState::kUndeliverable:
      _titleLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_UNDELIVERABLE_STATUS);
      _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
      // No status bars.
      [_firstStatusBar removeFromSuperview];
      [_secondStatusBar removeFromSuperview];
      [_thirdStatusBar removeFromSuperview];
      imageColorName = kGrey400Color;
      imageContainerColorName = kGrey100Color;
      break;
    case ParcelState::kReturnToSender:
    case ParcelState::kReturnCompleted:
      _titleLabel.text = l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_PACKAGE_RETURNED_TO_SENDER_STATUS);
      _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
      // No status bars.
      [_firstStatusBar removeFromSuperview];
      [_secondStatusBar removeFromSuperview];
      [_thirdStatusBar removeFromSuperview];
      imageColorName = kGrey400Color;
      imageContainerColorName = kGrey100Color;
      break;
    default:
      break;
  }

  if (!_useCarrierLogo) {
    _imageContainer.backgroundColor =
        [UIColor colorNamed:imageContainerColorName];
    _iconImageView.tintColor = [UIColor colorNamed:imageColorName];
  }
}

- (void)handleTap:(UITapGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateEnded) {
    [self.commandHandler loadParcelTrackingPage:_parcelTrackingURL];
  }
}

// Returns the icon container's border width.
- (CGFloat)iconBorderWidth {
  if (!_useCarrierLogo &&
      self.traitCollection.userInterfaceStyle != UIUserInterfaceStyleDark) {
    return 0;
  }
  return 1;
}

// Updates properties of some elements of the UI when UITraitUserInterfaceStyle
// or UITraitPreferredContentSizeCategories are changed.
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (previousTraitCollection.userInterfaceStyle !=
      self.traitCollection.userInterfaceStyle) {
    _imageContainer.layer.borderColor =
        [UIColor colorNamed:kGrey200Color].CGColor;
    _imageContainer.layer.borderWidth = [self iconBorderWidth];
  }
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    _titleLabel.font =
        CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold);
  }
}

#pragma mark - Testing category methods

- (NSString*)titleLabelTextForTesting {
  return self->_titleLabel.text;
}

@end
