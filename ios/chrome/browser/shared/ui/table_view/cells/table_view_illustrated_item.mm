// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_illustrated_item.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The insets of the View content and additional margin for some of its items.
const CGFloat kStackMargin = 32.0;
const CGFloat kItemMargin = 16.0;
// Spacing within stackView.
const CGFloat kStackViewSpacing = 13.0;
// Height of the image.
const CGFloat kImageViewHeight = 150.0;
// Horizontal Inset between button contents and edge.
const CGFloat kButtonTitleHorizontalContentInset = 40.0;
// Vertical Inset between button contents and edge.
const CGFloat kButtonTitleVerticalContentInset = 8.0;
// Button corner radius.
const CGFloat kButtonCornerRadius = 8.0;
}  // namespace

#pragma mark - TableViewIllustratedItem

@implementation TableViewIllustratedItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewIllustratedCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewIllustratedCell* cell =
      base::mac::ObjCCastStrict<TableViewIllustratedCell>(tableCell);
  if ([self.accessibilityIdentifier length]) {
    cell.accessibilityIdentifier = self.accessibilityIdentifier;
  }
  [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
  if (self.image) {
    cell.illustratedImageView.image = self.image;
  } else {
    cell.illustratedImageView.hidden = YES;
  }
  if ([self.title length]) {
    cell.titleLabel.text = self.title;
  } else {
    cell.titleLabel.hidden = YES;
  }
  if ([self.subtitle length]) {
    cell.subtitleLabel.text = self.subtitle;
  } else {
    cell.subtitleLabel.hidden = YES;
  }
  if ([self.buttonText length]) {
    [cell.button setTitle:self.buttonText forState:UIControlStateNormal];
  } else {
    cell.button.hidden = YES;
  }
  // Disable animations when setting the background color to prevent flash on
  // rotation.
  [UIView setAnimationsEnabled:NO];
  cell.backgroundColor = nil;
  [UIView setAnimationsEnabled:YES];

  if (styler.cellTitleColor) {
    cell.titleLabel.textColor = styler.cellTitleColor;
  }
}

@end

#pragma mark - TableViewIllustratedCell

@implementation TableViewIllustratedCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _illustratedImageView = [[UIImageView alloc] initWithImage:nil];
    _illustratedImageView.contentMode = UIViewContentModeScaleAspectFit;
    _illustratedImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _illustratedImageView.clipsToBounds = YES;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.numberOfLines = 0;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
    _subtitleLabel.textAlignment = NSTextAlignmentCenter;
    _subtitleLabel.numberOfLines = 0;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;

    _button = [[UIButton alloc] init];
    _button.backgroundColor = [UIColor colorNamed:kBlueColor];
    [_button.titleLabel
        setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
    _button.layer.cornerRadius = kButtonCornerRadius;
    _button.translatesAutoresizingMaskIntoConstraints = NO;

    // TODO(crbug.com/1418068): Simplify after minimum version required is >=
    // iOS 15.
    if (base::ios::IsRunningOnIOS15OrLater() &&
        IsUIButtonConfigurationEnabled()) {
      if (@available(iOS 15, *)) {
        UIButtonConfiguration* buttonConfiguration =
            [UIButtonConfiguration plainButtonConfiguration];
        buttonConfiguration.contentInsets =
            NSDirectionalEdgeInsetsMake(kButtonTitleVerticalContentInset,
                                        kButtonTitleHorizontalContentInset,
                                        kButtonTitleVerticalContentInset,
                                        kButtonTitleHorizontalContentInset);
        _button.configuration = buttonConfiguration;
      }
    } else {
      UIEdgeInsets contentInsets = UIEdgeInsetsMake(
          kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset,
          kButtonTitleVerticalContentInset, kButtonTitleHorizontalContentInset);
      SetContentEdgeInsets(_button, contentInsets);
    }

    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _illustratedImageView, _titleLabel, _subtitleLabel, _button
    ]];
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.spacing = kStackViewSpacing;
    stackView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:stackView];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_illustratedImageView.heightAnchor
          constraintEqualToConstant:kImageViewHeight],
      [_illustratedImageView.leadingAnchor
          constraintEqualToAnchor:stackView.leadingAnchor],
      [_illustratedImageView.trailingAnchor
          constraintEqualToAnchor:stackView.trailingAnchor],

      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:stackView.leadingAnchor],
      [_titleLabel.trailingAnchor
          constraintEqualToAnchor:stackView.trailingAnchor],

      // Subtitle should have additional margins on both sides
      [_subtitleLabel.leadingAnchor
          constraintEqualToAnchor:stackView.leadingAnchor
                         constant:kItemMargin],
      [_subtitleLabel.trailingAnchor
          constraintEqualToAnchor:stackView.trailingAnchor
                         constant:-kItemMargin],

      [stackView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kStackMargin],
      [stackView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kStackMargin],
      [stackView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
      [stackView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.illustratedImageView.hidden = NO;
  self.titleLabel.hidden = NO;
  self.subtitleLabel.hidden = NO;
  self.button.hidden = NO;

  [self.button removeTarget:nil
                     action:nil
           forControlEvents:UIControlEventAllEvents];
}

@end
