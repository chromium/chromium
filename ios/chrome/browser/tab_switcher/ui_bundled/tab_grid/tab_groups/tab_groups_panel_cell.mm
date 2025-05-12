// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/ui/tab_group_favicons_grid.h"
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

@implementation TabGroupsPanelCell {
  // The main stack view that contains subviews.
  UIStackView* _stackView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    _stackView = [self setUpStackView];
    [self.contentView addSubview:_stackView];

    _faviconsGrid = [[TabGroupFaviconsGrid alloc] init];
    _faviconsGrid.translatesAutoresizingMaskIntoConstraints = NO;
    [_stackView addArrangedSubview:_faviconsGrid];

    _dot = [[UIView alloc] init];
    _dot.layer.cornerRadius = kDotSize / 2;

    _titleLabel = [self setUpTitleLabel];
    _subtitleLabel = [self setUpSubtitleLabel];

    UIStackView* titleLabelWithDot =
        [[UIStackView alloc] initWithArrangedSubviews:@[ _dot, _titleLabel ]];
    titleLabelWithDot.alignment = UIStackViewAlignmentCenter;
    titleLabelWithDot.spacing = kSpacing;

    UIStackView* labelsStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ titleLabelWithDot, _subtitleLabel ]];
    labelsStackView.axis = UILayoutConstraintAxisVertical;
    labelsStackView.spacing = kSpacing;
    [_stackView addArrangedSubview:labelsStackView];

    AddSquareConstraints(_dot, kDotSize);
    AddSameCenterYConstraint(_faviconsGrid, self.contentView);
    AddSameCenterYConstraint(labelsStackView, self.contentView);
    AddSameConstraintsWithInset(_stackView, self.contentView, kMargin);
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
  self.facePile = nil;
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

#pragma mark - Private

// Sets up the stack view.
- (UIStackView*)setUpStackView {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.spacing = kSpacing;
  stackView.distribution = UIStackViewDistributionFill;
  return stackView;
}

// Sets up the title label.
- (UILabel*)setUpTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  return titleLabel;
}

// Sets up the subtitle label.
- (UILabel*)setUpSubtitleLabel {
  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  subtitleLabel.adjustsFontForContentSizeCategory = YES;
  subtitleLabel.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityMedium;
  return subtitleLabel;
}

#pragma mark - Setters

- (void)setFacePile:(UIView*)facePile {
  if (_facePile.superview == _stackView) {
    [_facePile removeFromSuperview];
  }

  _facePile = facePile;

  if (!facePile) {
    return;
  }

  facePile.translatesAutoresizingMaskIntoConstraints = NO;
  [_stackView addArrangedSubview:facePile];
}

@end
