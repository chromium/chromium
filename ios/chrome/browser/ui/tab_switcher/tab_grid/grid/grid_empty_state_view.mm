// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_state_view.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kVerticalMargin = 16.0;
const CGFloat kImageHeight = 150.0;
const CGFloat kImageWidth = 150.0;

// Returns the image to display for the given grid `page`.
UIImage* ImageForPage(TabGridPage page) {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return [UIImage imageNamed:@"tab_grid_incognito_tabs_empty"];
    case TabGridPageRegularTabs:
      return [UIImage imageNamed:@"tab_grid_regular_tabs_empty"];
    case TabGridPageRemoteTabs:
      // No-op. Empty page.
      break;
    case TabGridPageTabGroups:
      return [UIImage imageNamed:@"tab_grid_tab_groups_empty"];
  }
  return nil;
}

// Returns the title to display for the given grid `page` and `mode`.
NSString* TitleForPageAndMode(TabGridPage page, TabGridMode mode) {
  if (mode == TabGridMode::kSearch) {
    return l10n_util::GetNSString(IDS_IOS_TAB_GRID_SEARCH_RESULTS_EMPTY_TITLE);
  }

  switch (page) {
    case TabGridPageIncognitoTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_INCOGNITO_TABS_EMPTY_TITLE);
    case TabGridPageRegularTabs:
      return l10n_util::GetNSString(IDS_IOS_TAB_GRID_REGULAR_TABS_EMPTY_TITLE);
    case TabGridPageRemoteTabs:
      // No-op. Empty page.
      break;
    case TabGridPageTabGroups:
      return l10n_util::GetNSString(IDS_IOS_TAB_GRID_TAB_GROUPS_EMPTY_TITLE);
  }

  return nil;
}

// Returns the message to display for the given grid `page` and `mode`.
NSString* BodyTextForPageAndMode(TabGridPage page, TabGridMode mode) {
  if (mode == TabGridMode::kSearch) {
    return nil;
  }

  switch (page) {
    case TabGridPageIncognitoTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_INCOGNITO_TABS_EMPTY_MESSAGE);
    case TabGridPageRegularTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_REGULAR_TABS_EMPTY_MESSAGE);
    case TabGridPageRemoteTabs:
      // No-op. Empty page.
      break;
    case TabGridPageTabGroups:
      return l10n_util::GetNSString(IDS_IOS_TAB_GRID_TAB_GROUPS_EMPTY_MESSAGE);
  }

  return nil;
}

}  // namespace

@interface TabGridEmptyStateView ()
@property(nonatomic, strong) UIView* container;
@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) NSLayoutConstraint* scrollViewHeight;
@property(nonatomic, weak) UIImageView* imageView;
@property(nonatomic, weak) UILabel* topLabel;
@property(nonatomic, weak) UILabel* bottomLabel;
@end

@implementation TabGridEmptyStateView
// GridEmptyView properties.
@synthesize scrollViewContentInsets = _scrollViewContentInsets;
@synthesize activePage = _activePage;
@synthesize tabGridMode = _tabGridMode;

- (instancetype)initWithPage:(TabGridPage)page {
  if ((self = [super initWithFrame:CGRectZero])) {
    _activePage = page;
    _tabGridMode = TabGridMode::kNormal;
  }
  return self;
}

#pragma mark - GridEmptyView

- (void)setScrollViewContentInsets:(UIEdgeInsets)scrollViewContentInsets {
  _scrollViewContentInsets = scrollViewContentInsets;
  self.scrollView.contentInset = scrollViewContentInsets;
  self.scrollViewHeight.constant =
      scrollViewContentInsets.top + scrollViewContentInsets.bottom;
}

- (void)setTabGridMode:(TabGridMode)tabGridMode {
  if (_tabGridMode == tabGridMode) {
    return;
  }

  _tabGridMode = tabGridMode;

  self.topLabel.text = TitleForPageAndMode(_activePage, _tabGridMode);
  self.bottomLabel.text = BodyTextForPageAndMode(_activePage, _tabGridMode);
}

- (void)setActivePage:(TabGridPage)activePage {
  if (_activePage == activePage) {
    return;
  }

  _activePage = activePage;

  self.imageView.image = ImageForPage(_activePage);
  self.topLabel.text = TitleForPageAndMode(_activePage, _tabGridMode);
  self.bottomLabel.text = BodyTextForPageAndMode(_activePage, _tabGridMode);
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (newSuperview) {
    // The first time this moves to a superview, perform the view setup.
    if (self.subviews.count == 0)
      [self setupViews];
    [self.container.widthAnchor
        constraintLessThanOrEqualToAnchor:self.safeAreaLayoutGuide.widthAnchor]
        .active = YES;
    [self.container.centerXAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.centerXAnchor]
        .active = YES;
  }
}

#pragma mark - Private

- (void)setupViews {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  self.container = container;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrollView = scrollView;

  UILabel* topLabel = [[UILabel alloc] init];
  _topLabel = topLabel;
  topLabel.translatesAutoresizingMaskIntoConstraints = NO;
  topLabel.text = TitleForPageAndMode(_activePage, _tabGridMode);
  topLabel.textColor = UIColorFromRGB(kTabGridEmptyStateTitleTextColor);
  topLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  topLabel.adjustsFontForContentSizeCategory = YES;
  topLabel.numberOfLines = 0;
  topLabel.textAlignment = NSTextAlignmentCenter;

  UILabel* bottomLabel = [[UILabel alloc] init];
  _bottomLabel = bottomLabel;
  bottomLabel.translatesAutoresizingMaskIntoConstraints = NO;
  bottomLabel.text = BodyTextForPageAndMode(_activePage, _tabGridMode);
  bottomLabel.textColor = UIColorFromRGB(kTabGridEmptyStateBodyTextColor);
  bottomLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  bottomLabel.adjustsFontForContentSizeCategory = YES;
  bottomLabel.numberOfLines = 0;
  bottomLabel.textAlignment = NSTextAlignmentCenter;

  UIImageView* imageView =
      [[UIImageView alloc] initWithImage:ImageForPage(_activePage)];
  _imageView = imageView;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  [container addSubview:imageView];

  [container addSubview:topLabel];
  [container addSubview:bottomLabel];
  [scrollView addSubview:container];
  [self addSubview:scrollView];

  self.scrollViewHeight = VerticalConstraintsWithInset(
      container, scrollView,
      self.scrollViewContentInsets.top + self.scrollViewContentInsets.bottom);

  [NSLayoutConstraint activateConstraints:@[
    [imageView.topAnchor constraintEqualToAnchor:container.topAnchor],
    [imageView.widthAnchor constraintEqualToConstant:kImageWidth],
    [imageView.heightAnchor constraintEqualToConstant:kImageHeight],
    [imageView.centerXAnchor constraintEqualToAnchor:container.centerXAnchor],
  ]];

  [NSLayoutConstraint activateConstraints:@[
    [topLabel.topAnchor constraintEqualToAnchor:imageView.bottomAnchor
                                       constant:kVerticalMargin],
    [topLabel.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [topLabel.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [topLabel.bottomAnchor
        constraintEqualToAnchor:bottomLabel.topAnchor
                       constant:-kTabGridEmptyStateVerticalMargin],

    [bottomLabel.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [bottomLabel.trailingAnchor
        constraintEqualToAnchor:container.trailingAnchor],
    [bottomLabel.bottomAnchor constraintEqualToAnchor:container.bottomAnchor
                                             constant:-kVerticalMargin],

    [container.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [container.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],

    [scrollView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [scrollView.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor],
    [scrollView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ]];
}

@end
