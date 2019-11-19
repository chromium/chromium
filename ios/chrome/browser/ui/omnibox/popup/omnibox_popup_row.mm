// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Side (w or h) length for the leading image view.
const CGFloat kImageViewSizeUIRefresh = 28.0;
const CGFloat kImageViewCornerRadiusUIRefresh = 7.0;
const CGFloat kTrailingButtonTrailingMargin = 4;
const CGFloat kTrailingButtonSize = 48.0;
const CGFloat kLeadingPaddingIpad = 183;
const CGFloat kLeadingPaddingIpadCompact = 71;
}

@interface OmniboxPopupRow () {
  BOOL _incognito;
}

// Set the append button normal and highlighted images.
- (void)updateTrailingButtonImages;

@end

@implementation OmniboxPopupRow

@synthesize imageView = _imageView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  return [self initWithIncognito:NO];
}

- (instancetype)initWithIncognito:(BOOL)incognito {
  self = [super initWithStyle:UITableViewCellStyleDefault
              reuseIdentifier:@"OmniboxPopupRow"];
  if (self) {
    _incognito = incognito;

    self.isAccessibilityElement = YES;
    self.backgroundColor = UIColor.clearColor;
    self.selectedBackgroundView = [[UIView alloc] initWithFrame:CGRectZero];
    self.selectedBackgroundView.backgroundColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:kTableViewRowHighlightColor], _incognito,
        [UIColor colorNamed:kTableViewRowHighlightDarkColor]);

    _textTruncatingLabel =
        [[FadeTruncatingLabel alloc] initWithFrame:CGRectZero];
    _textTruncatingLabel.userInteractionEnabled = NO;
    [self.contentView addSubview:_textTruncatingLabel];

    _detailTruncatingLabel =
        [[FadeTruncatingLabel alloc] initWithFrame:CGRectZero];
    _detailTruncatingLabel.userInteractionEnabled = NO;
    [self.contentView addSubview:_detailTruncatingLabel];

    // Answers use a UILabel with NSLineBreakByTruncatingTail to produce a
    // truncation with an ellipse instead of fading on multi-line text.
    _detailAnswerLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _detailAnswerLabel.userInteractionEnabled = NO;
    _detailAnswerLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [self.contentView addSubview:_detailAnswerLabel];

    _trailingButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_trailingButton setContentMode:UIViewContentModeRight];
    [self updateTrailingButtonImages];
    // TODO(justincohen): Consider using the UITableViewCell's accessory view.
    // The current implementation is from before using a UITableViewCell.
    [self.contentView addSubview:_trailingButton];

    // Before UI Refresh, the leading icon is only displayed on iPad. In UI
    // Refresh, it's only in Regular x Regular size class.
    // TODO(justincohen): Consider using the UITableViewCell's image view.
    // The current implementation is from before using a UITableViewCell.
    _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _imageView.userInteractionEnabled = NO;
    _imageView.contentMode = UIViewContentModeCenter;

    _imageView.layer.cornerRadius = kImageViewCornerRadiusUIRefresh;
    _imageView.backgroundColor = UIColor.clearColor;
    _imageView.tintColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:@"omnibox_suggestion_icon_color"], _incognito,
        [UIColor colorNamed:@"omnibox_suggestion_icon_dark_color"]);

    _answerImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _answerImageView.userInteractionEnabled = NO;
    _answerImageView.contentMode = UIViewContentModeScaleAspectFit;
    [self.contentView addSubview:_answerImageView];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self layoutAccessoryViews];
  if ([self showsLeadingIcons]) {
    [self.contentView addSubview:_imageView];
  } else {
    [_imageView removeFromSuperview];
  }
}

- (void)layoutAccessoryViews {
  CGFloat imageViewSize = kImageViewSizeUIRefresh;
  LayoutRect imageViewLayout = LayoutRectMake(
      ([self showsLeadingIcons] && IsCompactTablet())
          ? kLeadingPaddingIpadCompact
          : kLeadingPaddingIpad,
      CGRectGetWidth(self.contentView.bounds),
      floor((_rowHeight - imageViewSize) / 2), imageViewSize, imageViewSize);
  _imageView.frame = LayoutRectGetRect(imageViewLayout);

  LayoutRect trailingAccessoryLayout =
      LayoutRectMake(CGRectGetWidth(self.contentView.bounds) -
                         kTrailingButtonSize - kTrailingButtonTrailingMargin,
                     CGRectGetWidth(self.contentView.bounds),
                     floor((_rowHeight - kTrailingButtonSize) / 2),
                     kTrailingButtonSize, kTrailingButtonSize);
  _trailingButton.frame = LayoutRectGetRect(trailingAccessoryLayout);
}

- (void)updateLeadingImage:(UIImage*)image {
  _imageView.image = image;

  // Adjust the vertical position based on the current size of the row.
  CGFloat imageViewSize = kImageViewSizeUIRefresh;
  CGRect frame = _imageView.frame;
  frame.origin.y = floor((_rowHeight - imageViewSize) / 2);
  _imageView.frame = frame;
}

- (void)setTabMatch:(BOOL)tabMatch {
  _tabMatch = tabMatch;
  [self updateTrailingButtonImages];
}

- (void)updateTrailingButtonImages {
  UIImage* appendImage = nil;
  _trailingButton.accessibilityIdentifier = nil;
  if (self.tabMatch) {
    appendImage = [UIImage imageNamed:@"omnibox_popup_tab_match"];
    appendImage = appendImage.imageFlippedForRightToLeftLayoutDirection;
    _trailingButton.accessibilityIdentifier =
        kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
  } else {
    int appendResourceID = 0;
    appendResourceID = IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND;
    appendImage = NativeReversableImage(appendResourceID, YES);
  }
  appendImage =
      [appendImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  _trailingButton.tintColor =
      color::DarkModeDynamicColor([UIColor colorNamed:kBlueColor], _incognito,
                                  [UIColor colorNamed:kBlueDarkColor]);

  [_trailingButton setImage:appendImage forState:UIControlStateNormal];
}

- (NSString*)accessibilityLabel {
  return _textTruncatingLabel.attributedText.string;
}

- (NSString*)accessibilityValue {
  return _detailTruncatingLabel.hidden
             ? _detailAnswerLabel.attributedText.string
             : _detailTruncatingLabel.attributedText.string;
}

- (BOOL)showsLeadingIcons {
    return IsRegularXRegularSizeClass();
}

- (void)accessibilityTrailingButtonTapped {
  [self.delegate accessibilityTrailingButtonTappedOmniboxPopupRow:self];
}

@end
