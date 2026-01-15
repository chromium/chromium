// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_tab_picker_empty_state_view.h"

#import "ios/chrome/browser/composebox/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kImageHeight = 190.0;
const CGFloat kImageWidth = 190.0;
}  // namespace

@implementation ComposeboxTabPickerEmptyStateView
// GridEmptyView properties.
@synthesize scrollViewContentInsets = _scrollViewContentInsets;
@synthesize activePage = _activePage;
@synthesize tabGridMode = _tabGridMode;

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (newSuperview) {
    // The first time this moves to a superview, perform the view setup.
    if (self.subviews.count == 0) {
      [self setupViews];
    }
  }
}

#pragma mark - Private

// Configures the view hierarchy and constraints.
// The scroll view supports dynamic type, allowing content to scroll when large
// and centering it when it fits the screen.
- (void)setupViews {
  self.accessibilityIdentifier =
      kComposeboxTabPickerEmptyStateViewAccessibilityIdentifier;
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_TAB_PICKER_EMPTY_STATE_TITLE);
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.numberOfLines = 0;
  titleLabel.textAlignment = NSTextAlignmentCenter;

  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  subtitleLabel.text = l10n_util::GetNSString(
      IDS_IOS_COMPOSEBOX_TAB_PICKER_EMPTY_STATE_SUBTITLE);
  subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  subtitleLabel.adjustsFontForContentSizeCategory = YES;
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.textAlignment = NSTextAlignmentCenter;

  UIImageView* imageView = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"composebox_tab_picker_empty"]];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* container = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, titleLabel, subtitleLabel ]];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  container.axis = UILayoutConstraintAxisVertical;
  container.alignment = UIStackViewAlignmentCenter;

  [scrollView addSubview:container];
  [self addSubview:scrollView];

  AddSizeConstraints(imageView, CGSizeMake(kImageWidth, kImageHeight));
  AddSameConstraints(scrollView.contentLayoutGuide, container);

  NSLayoutConstraint* scrollViewHeightConstraint = [scrollView.heightAnchor
      constraintGreaterThanOrEqualToAnchor:container.heightAnchor];
  scrollViewHeightConstraint.priority = UILayoutPriorityDefaultLow;

  NSLayoutConstraint* containerWidthConstraint =
      [container.widthAnchor constraintEqualToAnchor:self.widthAnchor];
  containerWidthConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor],
    [scrollView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.bottomAnchor],
    [scrollView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [scrollView.widthAnchor constraintEqualToAnchor:self.widthAnchor],
    containerWidthConstraint,
    scrollViewHeightConstraint,
  ]];
}

@end
