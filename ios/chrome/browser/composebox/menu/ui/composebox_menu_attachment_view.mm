// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The corner radius for the image view.
const CGFloat kImageContainerCornerRadius = 20.0f;

// The margin from the image background to the title.
const CGFloat kTitleMargin = 4.0f;

// The size of the icon.
const CGFloat kIconSize = 24.0f;

// The height of the image background view.
const CGFloat kImageBackgroundHeight = 60.0f;

// The base font size for the title label.
const CGFloat kTitleBaseFontSize = 13.0f;

// The maximum point size for the title label.
const CGFloat kTitleMaxPointSize = 24.0f;

}  // namespace

@implementation ComposeboxMenuAttachmentView {
  // The rounded corner background of the symbol.
  UIView* _imageBackgroundView;
  // View containing the image representing the attachment option.
  UIImageView* _imageView;
  // The label containing the tilte.
  UILabel* _titleLabel;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];

  if (self) {
    [self setUpImageView];
    [self setUpTitleLabel];
    [self setUpConstraints];
  }

  return self;
}

#pragma mark - Properties

- (void)setImage:(UIImage*)image {
  _image = image;
  _imageView.image = image;
}

- (NSString*)title {
  return _titleLabel.text;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
}

#pragma mark - Private

// Sets up the view contianing the image.
- (void)setUpImageView {
  _imageBackgroundView = [[UIView alloc] init];
  _imageBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageBackgroundView.userInteractionEnabled = NO;
  _imageBackgroundView.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  _imageBackgroundView.layer.cornerRadius = kImageContainerCornerRadius;
  [self addSubview:_imageBackgroundView];

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  _imageView.userInteractionEnabled = NO;
  [self addSubview:_imageView];

  AddSameCenterConstraints(_imageView, _imageBackgroundView);
}

// Sets up the view contianing the title.
- (void)setUpTitleLabel {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.textAlignment = NSTextAlignmentCenter;
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.numberOfLines = 2;
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  UIFont* baseFont = [UIFont systemFontOfSize:kTitleBaseFontSize
                                       weight:UIFontWeightRegular];
  _titleLabel.font =
      [[UIFontMetrics metricsForTextStyle:UIFontTextStyleFootnote]
          scaledFontForFont:baseFont
           maximumPointSize:kTitleMaxPointSize];
  [self addSubview:_titleLabel];
}

// Adds constraints to the subviews.
- (void)setUpConstraints {
  // TODO(crbug.com/504892651): Readjust based on dynamic type.
  AddSameConstraintsToSides(
      _imageBackgroundView, self,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kTop);
  AddSameConstraintsToSides(
      _titleLabel, self,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  AddSizeConstraints(_imageView, CGSizeMake(kIconSize, kIconSize));
  [NSLayoutConstraint activateConstraints:@[
    [_imageBackgroundView.bottomAnchor
        constraintEqualToAnchor:_titleLabel.topAnchor
                       constant:-kTitleMargin],
    [_imageBackgroundView.heightAnchor
        constraintEqualToConstant:kImageBackgroundHeight],
  ]];
}

@end
