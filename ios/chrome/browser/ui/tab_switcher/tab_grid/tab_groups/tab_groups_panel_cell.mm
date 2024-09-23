// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_favicon_grid.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kCornerRadius = 16;
const CGFloat kMargin = 16;
const CGFloat kSpacing = 8;
const CGFloat kDotSize = 14;

}  // namespace

@implementation TabGroupsPanelCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    _faviconsGrid = [[TabGroupsPanelFaviconGrid alloc] init];
    _faviconsGrid.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_faviconsGrid];

    _dot = [[UIView alloc] init];
    _dot.layer.cornerRadius = kDotSize / 2;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.maximumContentSizeCategory =
        UIContentSizeCategoryAccessibilityMedium;

    UIStackView* titleLabelWithDot =
        [[UIStackView alloc] initWithArrangedSubviews:@[ _dot, _titleLabel ]];
    titleLabelWithDot.alignment = UIStackViewAlignmentCenter;
    titleLabelWithDot.spacing = kSpacing;

    UIStackView* labels = [[UIStackView alloc]
        initWithArrangedSubviews:@[ titleLabelWithDot, _subtitleLabel ]];
    labels.axis = UILayoutConstraintAxisVertical;
    labels.spacing = kSpacing;
    labels.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:labels];

    AddSquareConstraints(_dot, kDotSize);
    AddSameCenterYConstraint(_faviconsGrid, self.contentView);
    AddSameCenterYConstraint(labels, self.contentView);
    [NSLayoutConstraint activateConstraints:@[
      [_faviconsGrid.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kMargin],
      [_faviconsGrid.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kMargin],
      [_faviconsGrid.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-kMargin],
      [labels.leadingAnchor constraintEqualToAnchor:_faviconsGrid.trailingAnchor
                                           constant:kMargin],
      [labels.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kMargin],
      [labels.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kMargin],
      [labels.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-kMargin],
    ]];
  }
  return self;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  NSString* colorName =
      highlighted ? kSecondaryBackgroundColor : kPrimaryBackgroundColor;
  self.backgroundColor = [UIColor colorNamed:colorName];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  _subtitleLabel.text = nil;
  _dot.backgroundColor = nil;
  _faviconsGrid.numberOfTabs = 0;
  _faviconsGrid.favicon1 = nil;
  _faviconsGrid.favicon2 = nil;
  _faviconsGrid.favicon3 = nil;
  _faviconsGrid.favicon4 = nil;
  self.item = nil;
}

- (NSString*)accessibilityLabel {
  NSString* numberOfTabsString = l10n_util::GetPluralNSStringF(
      IDS_IOS_TAB_GROUP_TABS_NUMBER, _faviconsGrid.numberOfTabs);
  return l10n_util::GetNSStringF(
      IDS_IOS_TAB_GROUPS_PANEL_CELL_ACCESSIBILITY_LABEL_FORMAT,
      base::SysNSStringToUTF16(_titleLabel.text),
      base::SysNSStringToUTF16(numberOfTabsString),
      base::SysNSStringToUTF16(_subtitleLabel.text));
}

@end
