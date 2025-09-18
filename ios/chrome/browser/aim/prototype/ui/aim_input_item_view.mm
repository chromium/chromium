// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_input_item_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// The input item max width.
const CGFloat kInputItemMaxWidth = 250.0f;
// The input item height.
const CGFloat kInputItemHeight = 42.0f;
// The input item padding.
const CGFloat kPadding = 10.0;
// The leading icon size.
const CGFloat kLeadingIconSize = 24.0;
// The preview image corner radius.
const CGFloat kPreviewImageCornerRadius = 9.0;
// The leading icon corner radius.
const CGFloat kLeadingIconCornerRadius = 6.0;
// Labels font size.
const CGFloat kLabelFontSize = 13.0;
// The preview image size.
const CGFloat kPreviewImageSize = 30.0;
// The preview image top and bottom padding.
const CGFloat kPreviewImageTopBottomPadding = 6.0;
}  // namespace

@interface AimInputItemView ()

/// Redefined internally as readwrite.
@property(nonatomic, strong, readwrite) UIButton* closeButton;

@end

@implementation AimInputItemView {
  // The leading icon for file/tab type of items.
  UIImageView* _leadingIconImageView;
  // The leading preview image view for image type of items.
  UIImageView* _previewImageView;
  // The title label for file/tab type of items.
  UILabel* _titleLabel;
  // The subtitle label for file/tab type of items.
  UILabel* _subtitleLabel;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupViews];
    [self setupConstraints];
  }
  return self;
}

- (void)configureWithItem:(AIMInputItem*)item {
  BOOL isImageItem = item.type == AIMInputItemType::kAIMInputItemTypeImage;

  _previewImageView.hidden = !isImageItem;
  _leadingIconImageView.hidden = isImageItem;
  _titleLabel.hidden = isImageItem;
  _subtitleLabel.hidden = isImageItem;

  if (isImageItem) {
    _previewImageView.image = item.previewImage;
  } else {
    if (item.type == AIMInputItemType::kAIMInputItemTypeFile) {
      _leadingIconImageView.image =
          DefaultSymbolWithPointSize(kDocSymbol, kLeadingIconSize);
    } else {
      _leadingIconImageView.image = item.leadingIconImage;
    }
    _titleLabel.text = item.title;
    _subtitleLabel.text = item.subtitle;
  }
}

- (void)setupViews {
  // Icon Image View
  _leadingIconImageView = [[UIImageView alloc] init];
  _leadingIconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _leadingIconImageView.backgroundColor = [UIColor whiteColor];
  _leadingIconImageView.layer.cornerRadius = kLeadingIconCornerRadius;
  _leadingIconImageView.clipsToBounds = YES;
  _leadingIconImageView.contentMode = UIViewContentModeScaleAspectFit;
  [self addSubview:_leadingIconImageView];

  // Title Label
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.font = PreferredFontForTextStyle(
      UIFontTextStyleFootnote, UIFontWeightMedium, kLabelFontSize);
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self addSubview:_titleLabel];

  // Subtitle Label
  _subtitleLabel = [[UILabel alloc] init];
  _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _subtitleLabel.font = PreferredFontForTextStyle(
      UIFontTextStyleFootnote, UIFontWeightMedium, kLabelFontSize);
  _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [_subtitleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self addSubview:_subtitleLabel];

  // Leading Image View
  _previewImageView = [[UIImageView alloc] init];
  _previewImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _previewImageView.contentMode = UIViewContentModeScaleAspectFill;
  _previewImageView.layer.cornerRadius = kPreviewImageCornerRadius;
  _previewImageView.clipsToBounds = YES;
  [self addSubview:_previewImageView];

  // Close Button

  _closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* image = SymbolWithPalette(
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kLeadingIconSize),
      @[ [UIColor colorNamed:kTextSecondaryColor], UIColor.whiteColor ]);
  [_closeButton setImage:image forState:UIControlStateNormal];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;

  [_closeButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self addSubview:_closeButton];

  self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.layer.cornerRadius = self.frame.size.height / 2;
  self.clipsToBounds = YES;
}

- (void)setupConstraints {
  [NSLayoutConstraint activateConstraints:@[
    [self.widthAnchor constraintLessThanOrEqualToConstant:kInputItemMaxWidth],
    [self.heightAnchor constraintEqualToConstant:kInputItemHeight],
    // leading icon ImageView
    [_leadingIconImageView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kPadding],
    [_leadingIconImageView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
    [_leadingIconImageView.widthAnchor
        constraintEqualToConstant:kLeadingIconSize],
    [_leadingIconImageView.heightAnchor
        constraintEqualToConstant:kLeadingIconSize],

    // Close Button
    [_closeButton.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                                constant:-kPadding],
    [_closeButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],

    // Title Label
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_leadingIconImageView.trailingAnchor
                       constant:kPadding],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                 constant:-kPadding],
    [_titleLabel.bottomAnchor constraintEqualToAnchor:self.centerYAnchor
                                             constant:-2.0],

    // Subtitle Label
    [_subtitleLabel.leadingAnchor
        constraintEqualToAnchor:_titleLabel.leadingAnchor],
    [_subtitleLabel.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
                                             constant:2.0],
    [_subtitleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                 constant:-kPadding],

    // Leading Image View
    [_previewImageView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                                    constant:kPadding],
    [_previewImageView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kPreviewImageTopBottomPadding],
    [_previewImageView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor
                       constant:-kPreviewImageTopBottomPadding],
    [_previewImageView.widthAnchor constraintEqualToConstant:kPreviewImageSize],
    [_previewImageView.heightAnchor
        constraintEqualToConstant:kPreviewImageSize],
    [_previewImageView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
  ]];
}

@end
