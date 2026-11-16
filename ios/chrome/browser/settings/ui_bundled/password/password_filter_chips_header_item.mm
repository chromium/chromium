// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_filter_chips_header_item.h"

#import <array>

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Ordered list of filter chips displayed in the header.
constexpr std::array<PasswordCredentialFilter, 3> kFilters = {
    PasswordCredentialFilter::kAll,
    PasswordCredentialFilter::kPasswords,
    PasswordCredentialFilter::kPasskeys,
};

// Returns the localized button title for `filter`.
NSString* TitleForFilter(PasswordCredentialFilter filter) {
  switch (filter) {
    case PasswordCredentialFilter::kAll:
      return l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_FILTER_ALL);
    case PasswordCredentialFilter::kPasswords:
      return l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_FILTER_PASSWORDS);
    case PasswordCredentialFilter::kPasskeys:
      return l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_FILTER_PASSKEYS);
  }
  NOTREACHED();
}

// Returns the accessibility identifier for `filter`.
NSString* AccessibilityIdentifierForFilter(PasswordCredentialFilter filter) {
  switch (filter) {
    case PasswordCredentialFilter::kAll:
      return kPasswordManagerFilterChipAllIdentifier;
    case PasswordCredentialFilter::kPasswords:
      return kPasswordManagerFilterChipPasswordsIdentifier;
    case PasswordCredentialFilter::kPasskeys:
      return kPasswordManagerFilterChipPasskeysIdentifier;
  }
  NOTREACHED();
}

}  // namespace

#pragma mark - PasswordFilterChipsHeaderItem

@implementation PasswordFilterChipsHeaderItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (!self) {
    return nil;
  }
  self.cellClass = [PasswordFilterChipsHeaderView class];
  _selectedFilter = PasswordCredentialFilter::kAll;
  _enabled = YES;
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter {
  [super configureHeaderFooterView:headerFooter];
  PasswordFilterChipsHeaderView* chipsView =
      base::apple::ObjCCastStrict<PasswordFilterChipsHeaderView>(headerFooter);
  [chipsView configureWithSelectedFilter:self.selectedFilter
                                 enabled:self.isEnabled];
}

@end

#pragma mark - PasswordFilterChipsHeaderView

@implementation PasswordFilterChipsHeaderView

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (!self) {
    return nil;
  }
  self.isAccessibilityElement = NO;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.clipsToBounds = NO;

  _chipButtons = [self createChipButtons];
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:_chipButtons];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.spacing = kTableViewSubViewHorizontalSpacing;

  [scrollView addSubview:stackView];
  [self.contentView addSubview:scrollView];

  CGFloat horizontalPadding = ChromeTableViewHorizontalPadding();
  AddSameConstraints(stackView, scrollView.contentLayoutGuide);
  AddSameConstraintsWithInsets(
      scrollView, self.contentView,
      NSDirectionalEdgeInsetsMake(kTableViewVerticalSpacing, horizontalPadding,
                                  kTableViewVerticalSpacing,
                                  horizontalPadding));
  [stackView.heightAnchor
      constraintEqualToAnchor:scrollView.frameLayoutGuide.heightAnchor]
      .active = YES;

  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.delegate = nil;
}

- (void)configureWithSelectedFilter:(PasswordCredentialFilter)selectedFilter
                            enabled:(BOOL)enabled {
  NSArray<ChromeButton*>* chipButtons = self.chipButtons;
  for (size_t i = 0; i < kFilters.size(); ++i) {
    ChromeButton* button = chipButtons[i];
    const BOOL isSelected = kFilters[i] == selectedFilter;
    button.style =
        isSelected ? ChromeButtonStylePrimary : ChromeButtonStyleSecondary;
    button.enabled = enabled;
    if (isSelected) {
      button.accessibilityTraits |= UIAccessibilityTraitSelected;
    } else {
      button.accessibilityTraits &= ~UIAccessibilityTraitSelected;
    }
  }
}

#pragma mark - Private

// Creates the filter chip buttons in `kFilters` order.
- (NSArray<ChromeButton*>*)createChipButtons {
  NSMutableArray<ChromeButton*>* buttons =
      [[NSMutableArray alloc] initWithCapacity:kFilters.size()];
  __weak __typeof(self) weakSelf = self;
  for (PasswordCredentialFilter filter : kFilters) {
    ChromeButton* button =
        [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary
                                 buttonSize:ChromeButtonSizeSmall];
    button.title = TitleForFilter(filter);
    button.accessibilityIdentifier = AccessibilityIdentifierForFilter(filter);
    [button
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [button addAction:[UIAction actionWithHandler:^(__kindof UIAction* action) {
              [weakSelf didTapFilter:filter];
            }]
        forControlEvents:UIControlEventTouchUpInside];
    [buttons addObject:button];
  }
  return buttons;
}

// Notifies the delegate that `filter` was tapped.
- (void)didTapFilter:(PasswordCredentialFilter)filter {
  [self.delegate passwordFilterChipsHeaderView:self didSelectFilter:filter];
}

@end
