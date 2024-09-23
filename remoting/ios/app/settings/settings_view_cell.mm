// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/settings/settings_view_cell.h"

#import <MaterialComponents/MaterialTypography.h>

#import "remoting/ios/app/remoting_theme.h"

static const CGFloat kImageViewSize = 24;
static const CGFloat kImageViewPaddingLeading = 16;
static const CGFloat kLabelViewsPaddingLeading = 16;
static const CGFloat kLabelViewsPaddingTrailing = 10;

@implementation SettingsViewCell {
  UIImageView* _imageView;
  UILabel* _titleLabel;
  UILabel* _detailsLabel;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self setupCell];
  }
  return self;
}

- (void)setupCell {
  self.isAccessibilityElement = YES;
  self.translatesAutoresizingMaskIntoConstraints = NO;

  _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_imageView];

  _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  UIFont* subheadFont = MDCTypography.subheadFont;
  UIFontDescriptor* subheadFontDescriptor = [subheadFont.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  subheadFontDescriptor = subheadFontDescriptor ?: subheadFont.fontDescriptor;
  UIFont* boldSubheadFont = [UIFont fontWithDescriptor:subheadFontDescriptor
                                                  size:subheadFont.pointSize];
  _titleLabel.font = boldSubheadFont;
  _titleLabel.textColor = RemotingTheme.menuTextColor;
  _titleLabel.numberOfLines = 1;
  [self addSubview:_titleLabel];

  _detailsLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _detailsLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _detailsLabel.font = MDCTypography.body1Font;
  _detailsLabel.textColor = RemotingTheme.menuTextColor;
  _detailsLabel.numberOfLines = 1;
  [self addSubview:_detailsLabel];

  UILayoutGuide* labelLayoutGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:labelLayoutGuide];
  [NSLayoutConstraint activateConstraints:@[
    [_imageView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_imageView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                             constant:kImageViewPaddingLeading],
    [_imageView.widthAnchor constraintEqualToConstant:kImageViewSize],
    [_imageView.heightAnchor constraintEqualToConstant:kImageViewSize],

    [labelLayoutGuide.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [labelLayoutGuide.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor
                       constant:kLabelViewsPaddingLeading],
    [labelLayoutGuide.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:kLabelViewsPaddingTrailing],

    [_titleLabel.topAnchor constraintEqualToAnchor:labelLayoutGuide.topAnchor],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:labelLayoutGuide.leadingAnchor],
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:labelLayoutGuide.trailingAnchor],

    [_detailsLabel.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor],
    [_detailsLabel.leadingAnchor
        constraintEqualToAnchor:labelLayoutGuide.leadingAnchor],
    [_detailsLabel.trailingAnchor
        constraintEqualToAnchor:labelLayoutGuide.trailingAnchor],
    [_detailsLabel.bottomAnchor
        constraintEqualToAnchor:labelLayoutGuide.bottomAnchor],
  ]];
}

- (void)setSettingOption:(SettingOption*)option {
  self.accessibilityLabel =
      option.subtext
          ? [NSString stringWithFormat:@"%@\n%@", option.title, option.subtext]
          : option.title;

  _titleLabel.text = option.title;
  _detailsLabel.text = option.subtext;

  switch (option.style) {
    case OptionCheckbox:
      if (option.checked) {
        _imageView.image = RemotingTheme.checkboxCheckedIcon;
      } else {
        _imageView.image = RemotingTheme.checkboxOutlineIcon;
      }
      break;
    case OptionSelector:
      if (option.checked) {
        _imageView.image = RemotingTheme.radioCheckedIcon;
      } else {
        _imageView.image = RemotingTheme.radioOutlineIcon;
      }
      break;
    case FlatButton:  // Fall-through.
    default:
      _imageView.image = [[UIImage alloc] init];
  }
}

@end
