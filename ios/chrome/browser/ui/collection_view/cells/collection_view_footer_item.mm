// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"

#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/string_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kDefaultHorizontalPadding = 24;

// Padding used on the leading edge for the text when the cell has an image.
const CGFloat kImageRightMargin = 10;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;
}  // namespace

@interface CollectionViewFooterCell ()

// Delegate to notify when the link in |textLabel| is tapped.
@property(nonatomic, weak) id<CollectionViewFooterLinkDelegate> linkDelegate;

// Sets the URL to load when the link in |textLabel| is tapped.
- (void)setLabelLinkURL:(const GURL&)URL
          withCellStyle:(CollectionViewCellStyle)cellStyle;

// Updates the cell's fonts and colors for the given |cellStyle| and uses
// dynamic types if they are available (iOS 11+).
- (void)updateForStyle:(CollectionViewCellStyle)cellStyle
       withFontScaling:(BOOL)withFontScaling;

@end

@implementation CollectionViewFooterItem

@synthesize text = _text;
@synthesize linkURL = _linkURL;
@synthesize linkDelegate = _linkDelegate;
@synthesize image = _image;
@synthesize cellStyle = _cellStyle;
@synthesize useScaledFont = _useScaledFont;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CollectionViewFooterCell class];
    _cellStyle = CollectionViewCellStyle::kMaterial;
    _linkURL = GURL();
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(CollectionViewFooterCell*)cell {
  [super configureCell:cell];

  // Update fonts and colors before setting the link label URL.
  [cell updateForStyle:_cellStyle withFontScaling:_useScaledFont];
  cell.textLabel.text = _text;
  [cell setLabelLinkURL:_linkURL withCellStyle:_cellStyle];
  cell.linkDelegate = _linkDelegate;
  cell.imageView.image = _image;
}

@end

@interface CollectionViewFooterCell () {
  LabelLinkController* _linkController;
  NSLayoutConstraint* _textLeadingAnchorConstraint;
  NSLayoutConstraint* _imageLeadingAnchorConstraint;
}
@end

@implementation CollectionViewFooterCell

@synthesize textLabel = _textLabel;
@synthesize imageView = _imageView;
@synthesize linkDelegate = _linkDelegate;
@synthesize horizontalPadding = _horizontalPadding;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    self.allowsCellInteractionsWhileEditing = YES;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.backgroundColor = [UIColor clearColor];
    _textLabel.numberOfLines = 0;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    [self.contentView addSubview:_textLabel];

    _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_imageView];

    _horizontalPadding = kDefaultHorizontalPadding;
    _textLeadingAnchorConstraint = [_textLabel.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor];
    _imageLeadingAnchorConstraint = [_imageView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:_horizontalPadding];
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                           constant:kVerticalPadding],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],
      [_imageView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      _textLeadingAnchorConstraint,
      _imageLeadingAnchorConstraint,
    ]];
  }
  return self;
}

- (void)setLabelLinkURL:(const GURL&)URL
          withCellStyle:(CollectionViewCellStyle)cellStyle {
  _linkController = nil;
  if (!URL.is_valid()) {
    return;
  }

  NSRange range;
  NSString* text = _textLabel.text;
  _textLabel.text = ParseStringWithLink(text, &range);
  if (range.location != NSNotFound && range.length != 0) {
    __weak CollectionViewFooterCell* weakSelf = self;
    _linkController = [[LabelLinkController alloc]
        initWithLabel:_textLabel
               action:^(const GURL& URL) {
                 [weakSelf.linkDelegate cell:weakSelf didTapLinkURL:URL];
               }];
    [_linkController setLinkColor:[UIColor colorNamed:kBlueColor]];
    [_linkController addLinkWithRange:range url:URL];
  }
}

- (void)updateForStyle:(CollectionViewCellStyle)cellStyle
       withFontScaling:(BOOL)withFontScaling {
  if (cellStyle == CollectionViewCellStyle::kUIKit) {
    self.textLabel.font = [UIFont systemFontOfSize:kUIKitFooterFontSize];
    self.textLabel.textColor = UIColor.cr_secondaryLabelColor;
  } else {
    self.textLabel.shadowOffset = CGSizeMake(1.f, 0.f);
    self.textLabel.shadowColor = [UIColor whiteColor];
    self.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    MaybeSetUILabelScaledFont(withFontScaling, self.textLabel,
                              [[MDCTypography fontLoader] mediumFontOfSize:14]);
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];

  _imageLeadingAnchorConstraint.constant = _horizontalPadding;

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = self.contentView.frame.size.width;
  if (_imageView.image) {
    _textLabel.preferredMaxLayoutWidth =
        parentWidth - 2.f * _imageLeadingAnchorConstraint.constant -
        kImageRightMargin - _imageView.image.size.width;
    _textLeadingAnchorConstraint.constant = kImageRightMargin;
  } else {
    _textLabel.preferredMaxLayoutWidth =
        parentWidth - 2.f * _imageLeadingAnchorConstraint.constant;
    _textLeadingAnchorConstraint.constant = 0;
  }

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.imageView.image = nil;
  [self setLabelLinkURL:GURL()
          withCellStyle:CollectionViewCellStyle::kMaterial];
  self.horizontalPadding = kDefaultHorizontalPadding;
  _linkController = nil;
  _linkDelegate = nil;
}

#pragma mark - Accessibility

- (BOOL)isAccessibilityElement {
  return NO;  // Accessibility for this element is handled in
              // LabelLinkController's TransparentLinkButton objects.
}

@end
