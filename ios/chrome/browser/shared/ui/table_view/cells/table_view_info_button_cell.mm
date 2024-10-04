// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Proportion of `textLayoutGuide` and `statusTextLabel`. This guarantees both
// of them at least occupies 20% of the cell.
const CGFloat kCellLabelsWidthProportion = 0.2f;

const CGFloat kInfoSymbolSize = 22;

}  // namespace

@interface TableViewInfoButtonCell ()

// UILabel displayed at the trailing side of the view, it's trailing anchor
// align with the leading anchor of the `bubbleView` below. It shows the status
// of the setting's row. Mostly show On or Off, but there is use case that shows
// a search engine name. Corresponding to `statusText` from the item.
@property(nonatomic, strong) UILabel* statusTextLabel;

// Views for the leading icon.
@property(nonatomic, readonly, strong) UIImageView* iconImageView;

// Constraints that are used when the iconImageView is visible and hidden.
@property(nonatomic, strong) NSLayoutConstraint* iconVisibleConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconHiddenConstraint;

// Constraints used when the `trailingButton` is visible and hidden.
@property(nonatomic, strong)
    NSLayoutConstraint* trailingButtonVisibleConstraint;
@property(nonatomic, strong) NSLayoutConstraint* trailingButtonHiddenConstraint;

// Constraints used based off of if `statusTextLabel` is visible.
@property(nonatomic, strong) NSArray* standardStatusTextLabelConstraints;
@property(nonatomic, strong) NSArray* accessibilityStatusTextLabelConstraints;

// Constraints that are used when the preferred content size is an
// "accessibility" category.
@property(nonatomic, strong) NSArray* accessibilityConstraints;
// Constraints that are used when the preferred content size is *not* an
// "accessibility" category.
@property(nonatomic, strong) NSArray* standardConstraints;
// Constraints that are used to set the top padding.
@property(nonatomic, strong) NSLayoutConstraint* topPaddingConstraint;
// Constraints that are used to set the bottom padding.
@property(nonatomic, strong) NSLayoutConstraint* bottomPaddingConstraint;

@end

@implementation TableViewInfoButtonCell {
  UIView* _iconBackground;
}

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    _isButtonSelectedForVoiceOver = YES;

    _iconBackground = [[UIView alloc] init];
    _iconBackground.translatesAutoresizingMaskIntoConstraints = NO;
    _iconBackground.hidden = YES;
    [self.contentView addSubview:_iconBackground];

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.contentMode = UIViewContentModeCenter;
    [_iconBackground addSubview:_iconImageView];

    AddSameCenterConstraints(_iconBackground, _iconImageView);

    UILayoutGuide* textLayoutGuide = [[UILayoutGuide alloc] init];
    [self.contentView addLayoutGuide:textLayoutGuide];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.numberOfLines = 0;
    [self.contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _detailTextLabel.numberOfLines = 0;
    [self.contentView addSubview:_detailTextLabel];

    _statusTextLabel = [[UILabel alloc] init];
    _statusTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _statusTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _statusTextLabel.adjustsFontForContentSizeCategory = YES;
    _statusTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [self.contentView addSubview:_statusTextLabel];

    _trailingButton = [UIButton buttonWithType:UIButtonTypeSystem];
    UIImage* infoImage =
        DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol, kInfoSymbolSize);
    [_trailingButton setImage:infoImage forState:UIControlStateNormal];
    _trailingButton.tintColor = [UIColor colorNamed:kBlueColor];
    _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_trailingButton
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _trailingButton.accessibilityIdentifier = kTableViewCellInfoButtonViewId;
    [self.contentView addSubview:_trailingButton];

    // Set up the constraint assuming that the button is hidden.
    _trailingButtonVisibleConstraint = [textLayoutGuide.trailingAnchor
        constraintLessThanOrEqualToAnchor:_trailingButton.leadingAnchor
                                 constant:-kTableViewHorizontalSpacing];
    _trailingButtonHiddenConstraint = [textLayoutGuide.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                 constant:-kTableViewHorizontalSpacing];

    // Set up the constraints assuming that the icon image is hidden.
    _iconVisibleConstraint = [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:_iconBackground.trailingAnchor
                       constant:kTableViewImagePadding];
    _iconHiddenConstraint = [textLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];

    // Set the constranits of `textLabel` and `statusTextLabel` to make their
    // width >= 20% of the cell to ensure both of them have a space.
    NSLayoutConstraint* widthConstraintStatus = [_statusTextLabel.widthAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.widthAnchor
                                  multiplier:kCellLabelsWidthProportion];
    widthConstraintStatus.priority = UILayoutPriorityDefaultHigh + 1;

    NSLayoutConstraint* widthConstraintLayoutGuide =
        [textLayoutGuide.widthAnchor
            constraintGreaterThanOrEqualToAnchor:self.contentView.widthAnchor
                                      multiplier:kCellLabelsWidthProportion];
    widthConstraintLayoutGuide.priority = UILayoutPriorityDefaultHigh + 1;

    // Set the content hugging property to `statusTextLabel` to wrap the text
    // and give other label more space.
    [_statusTextLabel
        setContentHuggingPriority:UILayoutPriorityDefaultHigh + 2
                          forAxis:UILayoutConstraintAxisHorizontal];

    _standardConstraints = @[
      [_trailingButton.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_trailingButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      _trailingButtonVisibleConstraint,
      [textLayoutGuide.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [textLayoutGuide.widthAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.widthAnchor
                                    multiplier:kCellLabelsWidthProportion],
      widthConstraintStatus,
      widthConstraintLayoutGuide,
    ];

    _accessibilityConstraints = @[
      [_trailingButton.topAnchor
          constraintEqualToAnchor:_statusTextLabel.bottomAnchor
                         constant:kTableViewLargeVerticalSpacing],
      [_trailingButton.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_trailingButton.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewLargeVerticalSpacing],
      _trailingButtonHiddenConstraint,
    ];

    _topPaddingConstraint = [textLayoutGuide.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:
                                        kTableViewOneLabelCellVerticalSpacing];
    _bottomPaddingConstraint = [self.contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:textLayoutGuide.bottomAnchor
                                    constant:
                                        kTableViewOneLabelCellVerticalSpacing];

    _standardStatusTextLabelConstraints = @[
      [_statusTextLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_statusTextLabel.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:textLayoutGuide.trailingAnchor
                                      constant:kTableViewHorizontalSpacing],
      [_statusTextLabel.trailingAnchor
          constraintEqualToAnchor:_trailingButton.leadingAnchor
                         constant:-kTableViewHorizontalSpacing]
    ];
    _accessibilityStatusTextLabelConstraints = @[
      [_statusTextLabel.topAnchor
          constraintEqualToAnchor:textLayoutGuide.bottomAnchor
                         constant:kTableViewLargeVerticalSpacing],
      [_statusTextLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_statusTextLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing]
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_iconBackground.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconBackground.widthAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_iconBackground.heightAnchor
          constraintEqualToAnchor:_iconBackground.widthAnchor],

      [_iconBackground.centerYAnchor
          constraintEqualToAnchor:textLayoutGuide.centerYAnchor],

      _iconHiddenConstraint,

      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [textLayoutGuide.leadingAnchor
          constraintEqualToAnchor:_detailTextLabel.leadingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor],
      [textLayoutGuide.trailingAnchor
          constraintEqualToAnchor:_detailTextLabel.trailingAnchor],
      [textLayoutGuide.topAnchor constraintEqualToAnchor:_textLabel.topAnchor],
      [textLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.bottomAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
      _topPaddingConstraint,
      _bottomPaddingConstraint,
    ]];

    if (UIContentSizeCategoryIsAccessibilityCategory(
            self.traitCollection.preferredContentSizeCategory)) {
      [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    } else {
      [NSLayoutConstraint activateConstraints:_standardConstraints];
    }

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(
          @[ UITraitPreferredContentSizeCategory.self ]);
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf updateConstraintsOnTraitChange:previousCollection];
      };
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

- (void)updatePaddingForDetailText:(BOOL)hasDetailText {
  if (hasDetailText) {
    self.topPaddingConstraint.constant = kTableViewTwoLabelsCellVerticalSpacing;
    self.bottomPaddingConstraint.constant =
        kTableViewTwoLabelsCellVerticalSpacing;
  } else {
    self.topPaddingConstraint.constant = kTableViewOneLabelCellVerticalSpacing;
    self.bottomPaddingConstraint.constant =
        kTableViewOneLabelCellVerticalSpacing;
  }
}

- (void)setIconImage:(UIImage*)image
           tintColor:(UIColor*)tintColor
     backgroundColor:(UIColor*)backgroundColor
        cornerRadius:(CGFloat)cornerRadius {
  if (tintColor) {
    image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }

  self.iconImageView.image = image;
  self.iconImageView.tintColor = tintColor;
  _iconBackground.layer.cornerRadius = cornerRadius;
  _iconBackground.backgroundColor = backgroundColor;

  BOOL hidden = (image == nil);
  _iconBackground.hidden = hidden;
  if (hidden) {
    self.iconVisibleConstraint.active = NO;
    self.iconHiddenConstraint.active = YES;
  } else {
    self.iconHiddenConstraint.active = NO;
    self.iconVisibleConstraint.active = YES;
  }
}

- (void)hideUIButton:(BOOL)isHidden {
  self.trailingButton.hidden = isHidden;
  self.trailingButtonHiddenConstraint.active = NO;
  if (isHidden) {
    self.trailingButtonHiddenConstraint.active = YES;
    self.trailingButtonVisibleConstraint.active = NO;
  } else {
    // In `standardConstraints`, `textLayoutGuide` is constrained to
    // trailingButton.leadingAnchor. In `accessibilityConstraints`,
    // trailingButton.leadingAnchor is constrained to contentView.leadingAnchor.
    // Therefore, if accessibility features are used, we should not activate a
    // constraint between `textLayoutGuide` and `trailingButton` since this
    // would mean the textLayoutGuide would be positioned before the
    // contentView.leadingAnchor.
    BOOL accessibilityEnabled = UIContentSizeCategoryIsAccessibilityCategory(
        self.traitCollection.preferredContentSizeCategory);
    self.trailingButtonHiddenConstraint.active = accessibilityEnabled;
    self.trailingButtonVisibleConstraint.active = !accessibilityEnabled;
  }
}

- (void)setStatusText:(NSString*)statusText {
  if (statusText) {
    _statusTextLabel.text = statusText;
    if (UIContentSizeCategoryIsAccessibilityCategory(
            self.traitCollection.preferredContentSizeCategory)) {
      [NSLayoutConstraint
          deactivateConstraints:_standardStatusTextLabelConstraints];
      [NSLayoutConstraint
          activateConstraints:_accessibilityStatusTextLabelConstraints];
    } else {
      [NSLayoutConstraint
          deactivateConstraints:_accessibilityStatusTextLabelConstraints];
      [NSLayoutConstraint
          activateConstraints:_standardStatusTextLabelConstraints];
    }
  }
}

#pragma mark - UIView

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateConstraintsOnTraitChange:previousTraitCollection];
}
#endif

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.statusTextLabel.text = nil;
  self.trailingButton.tag = 0;
  [self setIconImage:nil tintColor:nil backgroundColor:nil cornerRadius:0];
  [_trailingButton removeTarget:nil
                         action:nil
               forControlEvents:[_trailingButton allControlEvents]];
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  // Center the activation point over the info button, so that double-tapping
  // triggers to show the popover if `isButtonSelectedForVoiceOver` is
  // true.
  if (!self.isButtonSelectedForVoiceOver) {
    return self.center;
  }

  CGRect buttonFrame = UIAccessibilityConvertFrameToScreenCoordinates(
      self.trailingButton.frame, self);
  return CGPointMake(CGRectGetMidX(buttonFrame), CGRectGetMidY(buttonFrame));
}

- (NSString*)accessibilityHint {
  if (self.customizedAccessibilityHint.length) {
    return self.customizedAccessibilityHint;
  }
  return l10n_util::GetNSString(IDS_IOS_INFO_BUTTON_ACCESSIBILITY_HINT);
}

- (NSString*)accessibilityLabel {
  if (!self.detailTextLabel.text) {
    return self.textLabel.text;
  }
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.detailTextLabel.text];
}

- (NSString*)accessibilityValue {
  return self.statusTextLabel.text;
}

#pragma mark - Private

// Updates view's constraints when the devices traits are modified.
- (void)updateConstraintsOnTraitChange:
    (UITraitCollection*)previousTraitCollection {
  BOOL isCurrentContentSizeAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory) !=
      isCurrentContentSizeAccessibility) {
    if (isCurrentContentSizeAccessibility) {
      [NSLayoutConstraint deactivateConstraints:_standardConstraints];
      [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    } else {
      [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
      [NSLayoutConstraint activateConstraints:_standardConstraints];
    }
  }
}

@end
