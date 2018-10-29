// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/omnibox/truncating_attributed_label.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageDimensionLength = 19.0;
// Side (w or h) length for the leading image view.
const CGFloat kImageViewSizeUIRefresh = 28.0;
const CGFloat kImageViewCornerRadiusUIRefresh = 7.0;
const CGFloat kTrailingButtonTrailingMargin = 4;
const CGFloat kTrailingButtonSize = 48.0;
}

@interface OmniboxPopupRow () {
  BOOL _incognito;
}

// Set the append button normal and highlighted images.
- (void)updateTrailingButtonImages;

@end

@implementation OmniboxPopupRow

@synthesize textTruncatingLabel = _textTruncatingLabel;
@synthesize detailTruncatingLabel = _detailTruncatingLabel;
@synthesize detailAnswerLabel = _detailAnswerLabel;
@synthesize trailingButton = _trailingButton;
@synthesize answerImageView = _answerImageView;
@synthesize imageView = _imageView;
@synthesize rowHeight = _rowHeight;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  return [self initWithIncognito:NO];
}

- (instancetype)initWithIncognito:(BOOL)incognito {
  self = [super initWithStyle:UITableViewCellStyleDefault
              reuseIdentifier:@"OmniboxPopupRow"];
  if (self) {
    self.isAccessibilityElement = YES;
    self.backgroundColor = [UIColor clearColor];
    _incognito = incognito;

    _textTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _textTruncatingLabel.userInteractionEnabled = NO;
    [self.contentView addSubview:_textTruncatingLabel];

    _detailTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _detailTruncatingLabel.userInteractionEnabled = NO;
    [self.contentView addSubview:_detailTruncatingLabel];

    // Answers use a UILabel with NSLineBreakByTruncatingTail to produce a
    // truncation with an ellipse instead of fading on multi-line text.
    _detailAnswerLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _detailAnswerLabel.userInteractionEnabled = NO;
    _detailAnswerLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [self.contentView addSubview:_detailAnswerLabel];

    _trailingButton = [UIButton buttonWithType:UIButtonTypeCustom];
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

    if (IsUIRefreshPhase1Enabled()) {
      _imageView.layer.cornerRadius = kImageViewCornerRadiusUIRefresh;
      _imageView.backgroundColor = incognito
                                       ? [UIColor colorWithWhite:1 alpha:0.05]
                                       : [UIColor colorWithWhite:0 alpha:0.03];
      _imageView.tintColor = incognito ? [UIColor colorWithWhite:1 alpha:0.4]
                                       : [UIColor colorWithWhite:0 alpha:0.33];
    }

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
  CGFloat kLeadingPaddingIpad = 164;
  CGFloat kLeadingPaddingIpadCompact = 71;
  if (IsUIRefreshPhase1Enabled()) {
    kLeadingPaddingIpad = 183;
  }

  CGFloat imageViewSize = IsUIRefreshPhase1Enabled() ? kImageViewSizeUIRefresh
                                                     : kImageDimensionLength;
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
  CGFloat imageViewSize = IsUIRefreshPhase1Enabled() ? kImageViewSizeUIRefresh
                                                     : kImageDimensionLength;
  CGRect frame = _imageView.frame;
  frame.origin.y = floor((_rowHeight - imageViewSize) / 2);
  _imageView.frame = frame;
}

- (void)updateHighlightBackground:(BOOL)highlighted {
  // Set the background color to match the color of selected table view cells
  // when their selection style is UITableViewCellSelectionStyleGray.
  if (highlighted) {
    self.backgroundColor = _incognito ? [UIColor colorWithWhite:1 alpha:0.1]
                                      : [UIColor colorWithWhite:0 alpha:0.05];
  } else {
    self.backgroundColor = [UIColor clearColor];
  }
}

- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated {
  [super setHighlighted:highlighted animated:animated];
  [self updateHighlightBackground:highlighted];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [self updateHighlightBackground:highlighted];
}

- (void)setTabMatch:(BOOL)tabMatch {
  _tabMatch = tabMatch;
  [self updateTrailingButtonImages];
}

- (void)updateTrailingButtonImages {
  UIImage* appendImage = nil;
  if (self.tabMatch) {
    appendImage = [UIImage imageNamed:@"omnibox_popup_tab_match"];
  } else {
    int appendResourceID = _incognito
                               ? IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND_INCOGNITO
                               : IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND;
    appendImage = NativeReversableImage(appendResourceID, YES);
  }
  if (IsUIRefreshPhase1Enabled()) {
    appendImage =
        [appendImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _trailingButton.tintColor = _incognito
                                    ? [UIColor colorWithWhite:1 alpha:0.5]
                                    : [UIColor colorWithWhite:0 alpha:0.3];
  } else {
    int appendSelectedResourceID =
        _incognito ? IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND_INCOGNITO_HIGHLIGHTED
                   : IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND_HIGHLIGHTED;
    UIImage* appendImageSelected =
        NativeReversableImage(appendSelectedResourceID, YES);
    [_trailingButton setImage:appendImageSelected
                     forState:UIControlStateHighlighted];
  }

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
  if (IsUIRefreshPhase1Enabled()) {
    return IsRegularXRegularSizeClass();
  } else {
    return IsIPadIdiom();
  }
}

@end
