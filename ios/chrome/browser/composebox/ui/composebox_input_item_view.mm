// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_item_view.h"

#import "ios/chrome/browser/composebox/public/composebox_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// The input item padding.
const CGFloat kLeadingPadding = 10.0;
// The side padding of the icon to the text.
const CGFloat kIconTrailingPadding = 6.0;
// The leading icon size.
const CGFloat kLeadingIconSize = 16;
// The intrinsic padding of the PDF icon image.
const CGFloat kPDFIconIntrinsicPadding = 2;
// The leading icon corner radius.
const CGFloat kLeadingIconCornerRadius = 6.0;
// Labels font size.
const CGFloat kLabelFontSize = 13.0;
// The fade view width.
const CGFloat kFadeViewWidth = 20.0f;
/// The close button trailing.
const CGFloat kTrailingMargin = 8.0;
}  // namespace

@implementation ComposeboxInputItemView {
  // The leading icon for file/tab type of items.
  UIImageView* _leadingIconImageView;
  // The leading preview image view for image type of items.
  UIImageView* _previewImageView;
  // The title label for file/tab type of items.
  UILabel* _titleLabel;
  // The fade view for the title label.
  UIView* _fadeView;
  // The layer representing the fade at the trailing edge of title label.
  CAGradientLayer* _gradientLayer;
  // The theme for this view.
  ComposeboxTheme* _theme;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupViews];
    [self setupConstraints];
  }

  NSArray<UITrait>* traits =
      TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
  [self registerForTraitChanges:traits withAction:@selector(updateGradient)];

  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateFadeViewVisibility];
  _fadeView.layer.sublayers.firstObject.frame = _fadeView.bounds;
}

- (void)configureWithItem:(ComposeboxInputItem*)item
                    theme:(ComposeboxTheme*)theme {
  BOOL isImageItem =
      item.type == ComposeboxInputItemType::kComposeboxInputItemTypeImage;

  _theme = theme;

  _previewImageView.hidden = !isImageItem;
  _leadingIconImageView.hidden = isImageItem;
  _titleLabel.hidden = isImageItem;

  [self updateGradient];

  if (isImageItem) {
    _previewImageView.image = item.previewImage;
  } else {
    if (item.type == ComposeboxInputItemType::kComposeboxInputItemTypeFile) {
      UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
          configurationWithPointSize:kLeadingIconSize
                              weight:UIImageSymbolWeightMedium
                               scale:UIImageSymbolScaleLarge];
      UIImage* pdfSymbol = SymbolWithPalette(
          CustomSymbolWithConfiguration(kPDFFillSymbol, configuration),
          @[ theme.pdfSymbolColor ]);
      _leadingIconImageView.image = pdfSymbol;
      // The PDF symbol has a 2 points intrinsice padding. To normalize it to
      // `kLeadingIconSize`, apply a scale effect to the image view that does
      // notdisturb the other constraints relative to the image.
      CGFloat compensationScale =
          kLeadingIconSize / (kLeadingIconSize - kPDFIconIntrinsicPadding);
      _leadingIconImageView.transform = CGAffineTransformScale(
          CGAffineTransformIdentity, compensationScale, compensationScale);
    } else if (item.type ==
               ComposeboxInputItemType::kComposeboxInputItemTypeTab) {
      _leadingIconImageView.image =
          item.leadingIconImage
              ?: DefaultSymbolWithPointSize(kGlobeAmericasSymbol,
                                            kLeadingIconSize);
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
  _leadingIconImageView.backgroundColor = UIColor.clearColor;
  _leadingIconImageView.layer.cornerRadius = kLeadingIconCornerRadius;
  _leadingIconImageView.clipsToBounds = YES;
  _leadingIconImageView.contentMode = UIViewContentModeScaleAspectFit;
  [self addSubview:_leadingIconImageView];

  // Title Label
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.font = PreferredFontForTextStyle(
      UIFontTextStyleFootnote, UIFontWeightRegular, kLabelFontSize);
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.lineBreakMode = NSLineBreakByClipping;
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [self addSubview:_titleLabel];

  // Fade view
  _fadeView = [[UIView alloc] init];
  _fadeView.translatesAutoresizingMaskIntoConstraints = NO;
  _fadeView.hidden = YES;
  _gradientLayer = [CAGradientLayer layer];
  _gradientLayer.startPoint = CGPointMake(0.0, 0.5);
  _gradientLayer.endPoint = CGPointMake(1.0, 0.5);
  [_fadeView.layer insertSublayer:_gradientLayer atIndex:0];
  [self addSubview:_fadeView];

  // Leading Image View
  _previewImageView = [[UIImageView alloc] init];
  _previewImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _previewImageView.contentMode = UIViewContentModeScaleAspectFill;
  _previewImageView.layer.cornerRadius =
      composeboxAttachments::kAttachmentCornerRadius;
  _previewImageView.clipsToBounds = YES;
  [self addSubview:_previewImageView];

  self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.layer.cornerRadius = composeboxAttachments::kAttachmentCornerRadius;
  self.clipsToBounds = YES;
}

- (void)setupConstraints {
  [NSLayoutConstraint activateConstraints:@[
    [self.widthAnchor constraintLessThanOrEqualToConstant:
                          composeboxAttachments::kTabFileInputItemSize.width],
    [self.heightAnchor
        constraintEqualToConstant:composeboxAttachments::kTabFileInputItemSize
                                      .height],
    // leading icon ImageView
    [_leadingIconImageView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kLeadingPadding],
    [_leadingIconImageView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
    [_leadingIconImageView.widthAnchor
        constraintEqualToConstant:kLeadingIconSize],
    [_leadingIconImageView.heightAnchor
        constraintEqualToConstant:kLeadingIconSize],

    // Title Label
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_leadingIconImageView.trailingAnchor
                       constant:kIconTrailingPadding],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                 constant:-kTrailingMargin],
    [_titleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],

    // Fade view
    [_fadeView.trailingAnchor
        constraintEqualToAnchor:_titleLabel.trailingAnchor],
    [_fadeView.topAnchor constraintEqualToAnchor:_titleLabel.topAnchor],
    [_fadeView.bottomAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor],
    [_fadeView.widthAnchor constraintEqualToConstant:kFadeViewWidth],
  ]];

  AddSameConstraints(_previewImageView, self);
}

- (void)updateGradient {
  _gradientLayer.colors = @[
    (id)[_theme.inputItemBackgroundColor colorWithAlphaComponent:0.0].CGColor,
    (id)_theme.inputItemBackgroundColor.CGColor
  ];
}

@end
