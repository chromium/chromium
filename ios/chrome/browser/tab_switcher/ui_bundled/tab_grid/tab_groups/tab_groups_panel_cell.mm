// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/ui/tab_group_favicons_grid.h"
#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_providing.h"
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

@interface TabGroupsPanelCell ()

// The face pile view.
@property(nonatomic, strong) UIView* facePile;

@end

@implementation TabGroupsPanelCell {
  // The main stack view that contains subviews.
  UIStackView* _stackView;
  // Spacer view used when `facePile` is added to the `_stackView`.
  UIView* _spacerFacePileView;
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

    UIView* contentView = self.contentView;

    AddSquareConstraints(_dot, kDotSize);
    AddSameCenterYConstraint(_faviconsGrid, contentView);
    AddSameCenterYConstraint(labelsStackView, contentView);
    [NSLayoutConstraint activateConstraints:@[
      [_stackView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kMargin],
      [_stackView.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                           constant:kMargin],
      [_stackView.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor
                                              constant:-kMargin],
      [_stackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
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
  self.facePileProvider = nil;
  self.item = nil;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  NSString* numberOfTabsString = l10n_util::GetPluralNSStringF(
      IDS_IOS_TAB_GROUP_TABS_NUMBER, _faviconsGrid.numberOfTabs);
  if (self.facePile) {
    return l10n_util::GetNSStringF(
        IDS_IOS_TAB_GROUPS_PANEL_CELL_SHARED_ACCESSIBILITY_LABEL_FORMAT,
        base::SysNSStringToUTF16(_titleLabel.text),
        base::SysNSStringToUTF16(numberOfTabsString),
        base::SysNSStringToUTF16(_subtitleLabel.text));
  }
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

- (void)setFacePileProvider:(id<FacePileProviding>)facePileProvider {
  if ([_facePileProvider isEqualFacePileProviding:facePileProvider]) {
    return;
  }
  _facePileProvider = facePileProvider;

  self.facePile = [_facePileProvider facePileView];
}

- (void)setFacePile:(UIView*)facePile {
  if ([_facePile isDescendantOfView:self]) {
    [_facePile removeFromSuperview];
  }
  if ([_spacerFacePileView isDescendantOfView:self]) {
    [_spacerFacePileView removeFromSuperview];
  }

  _facePile = facePile;

  if (!facePile) {
    return;
  }

  if (!_spacerFacePileView) {
    _spacerFacePileView = [[UIView alloc] init];
    _spacerFacePileView.translatesAutoresizingMaskIntoConstraints = NO;
  }

  facePile.translatesAutoresizingMaskIntoConstraints = NO;
  [facePile setContentHuggingPriority:UILayoutPriorityRequired
                              forAxis:UILayoutConstraintAxisHorizontal];
  [facePile
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  [_spacerFacePileView
      setContentHuggingPriority:UILayoutPriorityFittingSizeLevel
                        forAxis:UILayoutConstraintAxisHorizontal];
  [_spacerFacePileView
      setContentCompressionResistancePriority:UILayoutPriorityFittingSizeLevel
                                      forAxis:UILayoutConstraintAxisHorizontal];

  [_stackView addArrangedSubview:_spacerFacePileView];
  [_stackView addArrangedSubview:facePile];
}

@end
