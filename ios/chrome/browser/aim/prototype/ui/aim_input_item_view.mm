// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_input_item_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

const CGSize kImageInputItemSize = {72.0f, 36.0f};
const CGSize kTabFileInputItemSize = {136.0f, 36.0f};

namespace {
// The input item padding.
const CGFloat kPadding = 10.0;
// The leading icon size.
const CGFloat kLeadingIconSize = 16;
// The preview image corner radius.
const CGFloat kPreviewImageCornerRadius = 9.0;
// The leading icon corner radius.
const CGFloat kLeadingIconCornerRadius = 6.0;
// Labels font size.
const CGFloat kLabelFontSize = 13.0;
// The preview image size.
const CGFloat kPreviewImageSize = 28.0;
// The preview image top and bottom padding.
const CGFloat kPreviewImageTopBottomPadding = 4.0;
/// The fade view width.
const CGFloat kFadeViewWidth = 20.0f;
/// The title to button padding.
const CGFloat kTitleCloseButtonPadding = 6.0;
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
  // The fade view for the title label.
  UIView* _fadeView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupViews];
    [self setupConstraints];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateFadeViewVisibility];
  _fadeView.layer.sublayers.firstObject.frame = _fadeView.bounds;
}

- (void)configureWithItem:(AIMInputItem*)item {
  BOOL isImageItem = item.type == AIMInputItemType::kAIMInputItemTypeImage;

  _previewImageView.hidden = !isImageItem;
  _leadingIconImageView.hidden = isImageItem;
  _titleLabel.hidden = isImageItem;

  if (isImageItem) {
    _previewImageView.image = item.previewImage;
  } else {
    if (item.type == AIMInputItemType::kAIMInputItemTypeFile) {
      _leadingIconImageView.image =
          DefaultSymbolWithPointSize(kTextDocument, kLeadingIconSize);
    } else {
      _leadingIconImageView.image = item.leadingIconImage;
    }
    _titleLabel.text = item.title;
  }
  [self updateFadeViewVisibility];
}

- (void)updateFadeViewVisibility {
  if (_titleLabel.intrinsicContentSize.width > _titleLabel.bounds.size.width) {
    _fadeView.hidden = NO;
  } else {
    _fadeView.hidden = YES;
  }
}

- (void)prepareForReuse {
  _leadingIconImageView.image = nil;
  _previewImageView.image = nil;
  _titleLabel.text = nil;
}

- (void)setupViews {
  // Icon Image View
  _leadingIconImageView = [[UIImageView alloc] init];
  _leadingIconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _leadingIconImageView.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  _leadingIconImageView.layer.cornerRadius = kLeadingIconCornerRadius;
  _leadingIconImageView.clipsToBounds = YES;
  _leadingIconImageView.contentMode = UIViewContentModeScaleAspectFit;
  [self addSubview:_leadingIconImageView];

  // Title Label
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.font = PreferredFontForTextStyle(
      UIFontTextStyleFootnote, UIFontWeightRegular, kLabelFontSize);
  _titleLabel.textColor = UIColor.blackColor;
  _titleLabel.lineBreakMode = NSLineBreakByClipping;
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self addSubview:_titleLabel];

  // Fade view
  _fadeView = [[UIView alloc] init];
  _fadeView.translatesAutoresizingMaskIntoConstraints = NO;
  _fadeView.hidden = YES;
  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.colors = @[
    (id)[[UIColor colorNamed:kSecondaryBackgroundColor]
        colorWithAlphaComponent:0.0]
        .CGColor,
    (id)[UIColor colorNamed:kSecondaryBackgroundColor].CGColor
  ];
  gradientLayer.startPoint = CGPointMake(0.0, 0.5);
  gradientLayer.endPoint = CGPointMake(1.0, 0.5);
  [_fadeView.layer insertSublayer:gradientLayer atIndex:0];
  [self addSubview:_fadeView];

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
      DefaultSymbolWithPointSize(kXMarkSymbol, kLeadingIconSize),
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
    [self.widthAnchor
        constraintLessThanOrEqualToConstant:kTabFileInputItemSize.width],
    [self.heightAnchor constraintEqualToConstant:kTabFileInputItemSize.height],
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
                                                constant:-13],
    [_closeButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],

    // Title Label
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_leadingIconImageView.trailingAnchor
                       constant:kPadding],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                 constant:-kTitleCloseButtonPadding],
    [_titleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],

    // Fade view
    [_fadeView.trailingAnchor
        constraintEqualToAnchor:_titleLabel.trailingAnchor],
    [_fadeView.topAnchor constraintEqualToAnchor:_titleLabel.topAnchor],
    [_fadeView.bottomAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor],
    [_fadeView.widthAnchor constraintEqualToConstant:kFadeViewWidth],

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
