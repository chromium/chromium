// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/cells/whats_new_table_view_banner_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The size of the margin between the top of the content view and the stack view
// or the bottom of the content view and the stack view.
const CGFloat kStackViewMargin = 16.0;
// The size of the leading margin between content view and the text labels.
const CGFloat kItemLeadingMargin = 16.0;
// The height of the image.
const CGFloat kImageViewHeight = 220.0;
// The size of the space between each items in the stack view.
const CGFloat kStackViewVerticalSpacings = 10.0;
// The size of the margin between the stack view and the content top view.
const CGFloat kTopMargin = 24.0;

}  // namespace

#pragma mark - WhatsNewTableViewBannerItem

@implementation WhatsNewTableViewBannerItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [WhatsNewTableViewBannerCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  WhatsNewTableViewBannerCell* cell =
      base::mac::ObjCCastStrict<WhatsNewTableViewBannerCell>(tableCell);
  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];

  self.accessibilityTraits |= UIAccessibilityTraitButton;

  cell.sectionTextLabel.text = self.sectionTitle;
  cell.textLabel.text = self.title;
  cell.detailTextLabel.text = self.detailText;

  if (!self.bannerImage) {
    cell.bannerImageView.hidden = YES;
    [cell setEmptyBannerImage];
    return;
  }

  cell.bannerImageView.image = self.bannerImage;
  if (self.isBannerAtBottom) {
    [cell setBannerImageAtBottom];
  }
}

@end

#pragma mark - WhatsNewTableViewBannerCell

@interface WhatsNewTableViewBannerCell ()

@property(nonatomic, strong) NSLayoutConstraint* stackViewTopAnchorConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* stackViewBottomAnchorConstraint;
@property(nonatomic, strong) UIStackView* stackView;
@property(nonatomic, strong) UIView* bannerView;
@property(nonatomic, assign) BOOL isBannerAtBottom;

@end

@implementation WhatsNewTableViewBannerCell

@synthesize detailTextLabel = _detailTextLabel;
@synthesize textLabel = _textLabel;
@synthesize sectionTextLabel = _sectionTextLabel;
@synthesize isBannerAtBottom = _isBannerAtBottom;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    // Banner View.
    self.bannerView = [[UIView alloc] init];

    // Banner image.
    _bannerImageView = [[UIImageView alloc] init];
    _bannerImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _isBannerAtBottom = NO;
    _bannerImageView.contentMode = UIViewContentModeScaleAspectFit;
    [self.bannerView addSubview:_bannerImageView];
    self.bannerView.backgroundColor =
        [UIColor colorNamed:@"hero_image_background_color"];

    // Text label.
    _textLabel = [[UILabel alloc] init];
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.font =
        CreateDynamicFont(UIFontTextStyleTitle1, UIFontWeightBold);
    _textLabel.numberOfLines = 2;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;

    // Detail text label.
    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _detailTextLabel.font =
        CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightRegular);
    _detailTextLabel.numberOfLines = 5;
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;

    // Section text label.
    _sectionTextLabel = [[UILabel alloc] init];
    _sectionTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _sectionTextLabel.font =
        CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
    _sectionTextLabel.numberOfLines = 1;
    _sectionTextLabel.translatesAutoresizingMaskIntoConstraints = NO;

    // Stack view.
    _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      self.bannerView, _sectionTextLabel, _textLabel, _detailTextLabel
    ]];
    _stackView.axis = UILayoutConstraintAxisVertical;
    _stackView.alignment = UIStackViewAlignmentLeading;
    _stackView.spacing = kStackViewVerticalSpacings;
    _stackView.distribution = UIStackViewDistributionFill;
    _stackView.alignment = UIStackViewAlignmentFill;
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_stackView];
    [_stackView setCustomSpacing:kTopMargin afterView:self.bannerView];

    // Stack view top and bottom anchor constraint.
    _stackViewTopAnchorConstraint = [_stackView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor];
    _stackViewBottomAnchorConstraint = [_stackView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:-kStackViewMargin];

    [NSLayoutConstraint activateConstraints:@[
      // Banner view constraints.
      [self.bannerView.heightAnchor constraintEqualToConstant:kImageViewHeight],
      [self.bannerView.widthAnchor
          constraintEqualToAnchor:_stackView.widthAnchor],
      [self.bannerView.leadingAnchor
          constraintEqualToAnchor:_stackView.leadingAnchor],
      [self.bannerView.trailingAnchor
          constraintEqualToAnchor:_stackView.trailingAnchor],

      // Banner image constraints.
      [_bannerImageView.centerYAnchor
          constraintEqualToAnchor:self.bannerView.centerYAnchor],
      [_bannerImageView.centerXAnchor
          constraintEqualToAnchor:self.bannerView.centerXAnchor],
      [_bannerImageView.heightAnchor
          constraintEqualToConstant:kImageViewHeight],

      // Section text label constraints.
      [_sectionTextLabel.leadingAnchor
          constraintEqualToAnchor:_stackView.leadingAnchor
                         constant:kItemLeadingMargin],
      [_sectionTextLabel.trailingAnchor
          constraintEqualToAnchor:_stackView.trailingAnchor
                         constant:-kItemLeadingMargin],

      // Text label constraints.
      [_textLabel.leadingAnchor constraintEqualToAnchor:_stackView.leadingAnchor
                                               constant:kItemLeadingMargin],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:_stackView.trailingAnchor
                         constant:-kItemLeadingMargin],

      // detail text label constraints.
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:_stackView.leadingAnchor
                         constant:kItemLeadingMargin],
      [_detailTextLabel.trailingAnchor
          constraintEqualToAnchor:_stackView.trailingAnchor
                         constant:-kItemLeadingMargin],

      // Stack view constraints.
      [_stackView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_stackView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      _stackViewTopAnchorConstraint,
      _stackViewBottomAnchorConstraint,
    ]];
  }
  return self;
}

- (void)setBannerImageAtBottom {
  [self.stackView addArrangedSubview:self.bannerView];

  // Update stack view constraints to remove margin at the bottom and add margin
  // at the top of the content view.
  self.stackViewTopAnchorConstraint.constant = kTopMargin;
  self.stackViewBottomAnchorConstraint.constant = 0.0;
  self.isBannerAtBottom = YES;
}

- (void)setBannerImageAtTop {
  [self.stackView insertArrangedSubview:self.bannerView atIndex:0];

  // Update stack view constraints to add margin at the bottom and remove margin
  // at the top of the content view.
  self.stackViewTopAnchorConstraint.constant = 0.0;
  [self.stackView setCustomSpacing:kTopMargin afterView:self.bannerView];
  self.stackViewBottomAnchorConstraint.constant = -kStackViewMargin;
  self.isBannerAtBottom = NO;
}

- (void)setEmptyBannerImage {
  // Add margin at the top and bottom of the stack view.
  self.stackViewTopAnchorConstraint.constant = kTopMargin;
  self.stackViewBottomAnchorConstraint.constant = -kStackViewMargin;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  self.sectionTextLabel.text = nil;
  self.bannerImageView.image = nil;
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.bannerImageView.hidden = NO;
  [self setBannerImageAtTop];
}

#pragma mark - Private

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

@end
