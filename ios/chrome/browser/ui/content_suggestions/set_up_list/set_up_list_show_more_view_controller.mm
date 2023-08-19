// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_show_more_view_controller.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_show_more_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Height of the separator between each Set Up List item.
const CGFloat kSeparatorHeight = 0.5;

// View spacing configurations.
const CGFloat kTitleDescriptionHorizontalInsets = 24.0f;
const CGFloat kTitleDescriptionVerticalSpacing = 7.0f;
const CGFloat kSetUpListItemSpacing = 5.0f;
const CGFloat kSetUpListStackViewLeadingInset = 31.0f;
const CGFloat kSetUpListStackViewTrailingInset = 20.0f;
const CGFloat kSetUpListStackViewDescriptionSpacing = 33.0f;

}  // namespace

@interface SetUpListShowMoreViewController ()
@end

@implementation SetUpListShowMoreViewController {
  NSArray<SetUpListItemViewData*>* _items;
  id<SetUpListViewDelegate> _tapDelegate;
}

- (instancetype)initWithItems:(NSArray<SetUpListItemViewData*>*)items
                  tapDelegate:(id<SetUpListViewDelegate>)tapDelegate {
  self = [super init];
  if (self) {
    _items = items;
    _tapDelegate = tapDelegate;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Determines background color of the entire view.
  UIView* backgroundView = [[UIView alloc] init];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  [self.view addSubview:backgroundView];
  AddSameConstraints(backgroundView, self.view);

  UINavigationBar* navigationBar = [[UINavigationBar alloc] init];
  navigationBar.translucent = NO;
  [navigationBar setShadowImage:[[UIImage alloc] init]];
  [navigationBar setBarTintColor:[UIColor colorNamed:kPrimaryBackgroundColor]];
  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:_tapDelegate
                           action:@selector(dismissSeeMoreViewController)];
  navigationItem.rightBarButtonItem = dismissButton;
  navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [navigationBar setItems:@[ navigationItem ]];
  [self.view addSubview:navigationBar];
  AddSameConstraintsToSides(
      navigationBar, self.view.safeAreaLayoutGuide,
      LayoutSides::kTrailing | LayoutSides::kTop | LayoutSides::kLeading);

  UILabel* title = [[UILabel alloc] init];
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.text = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE);
  title.accessibilityIdentifier =
      l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE);
  title.font = CreateDynamicFont(UIFontTextStyleTitle1, UIFontWeightBold);
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.numberOfLines = 0;
  title.lineBreakMode = NSLineBreakByWordWrapping;
  title.textAlignment = NSTextAlignmentCenter;
  [self.view addSubview:title];

  UILabel* subtitle = [[UILabel alloc] init];
  subtitle.translatesAutoresizingMaskIntoConstraints = NO;
  subtitle.text = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_DESCRIPTION);
  subtitle.accessibilityIdentifier =
      l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_DESCRIPTION);
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitle.numberOfLines = 0;
  subtitle.lineBreakMode = NSLineBreakByWordWrapping;
  subtitle.textAlignment = NSTextAlignmentCenter;
  [self.view addSubview:subtitle];
  [NSLayoutConstraint activateConstraints:@[
    [title.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kTitleDescriptionHorizontalInsets],
    [title.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kTitleDescriptionHorizontalInsets],
    [title.topAnchor constraintEqualToAnchor:navigationBar.bottomAnchor],
    [subtitle.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kTitleDescriptionHorizontalInsets],
    [subtitle.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kTitleDescriptionHorizontalInsets],
    [subtitle.topAnchor
        constraintEqualToAnchor:title.bottomAnchor
                       constant:kTitleDescriptionVerticalSpacing],
  ]];

  UIStackView* setUpListItemStackView = [[UIStackView alloc] init];
  setUpListItemStackView.axis = UILayoutConstraintAxisVertical;
  setUpListItemStackView.alignment = UIStackViewAlignmentLeading;
  setUpListItemStackView.distribution = UIStackViewDistributionFill;
  setUpListItemStackView.translatesAutoresizingMaskIntoConstraints = NO;
  setUpListItemStackView.spacing = kSetUpListItemSpacing;

  for (SetUpListShowMoreItemView* view in [self createItems]) {
    [setUpListItemStackView addArrangedSubview:view];
    UIView* separator = [[UIView alloc] init];
    separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
    [setUpListItemStackView addArrangedSubview:separator];
    [NSLayoutConstraint activateConstraints:@[
      [separator.heightAnchor
          constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
      [separator.leadingAnchor
          constraintEqualToAnchor:setUpListItemStackView.leadingAnchor],
      [separator.trailingAnchor
          constraintEqualToAnchor:setUpListItemStackView.trailingAnchor],
    ]];
  }
  [self.view addSubview:setUpListItemStackView];
  [NSLayoutConstraint activateConstraints:@[
    [setUpListItemStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kSetUpListStackViewLeadingInset],
    [setUpListItemStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kSetUpListStackViewTrailingInset],
    [setUpListItemStackView.topAnchor
        constraintEqualToAnchor:subtitle.bottomAnchor
                       constant:kSetUpListStackViewDescriptionSpacing],
  ]];
}

#pragma mark - helpers

- (NSMutableArray<SetUpListShowMoreItemView*>*)createItems {
  NSMutableArray<SetUpListShowMoreItemView*>* items =
      [[NSMutableArray alloc] init];
  NSMutableArray<SetUpListShowMoreItemView*>* completed =
      [[NSMutableArray alloc] init];

  for (SetUpListItemViewData* data in _items) {
    SetUpListShowMoreItemView* view =
        [[SetUpListShowMoreItemView alloc] initWithData:data];
    view.tapDelegate = _tapDelegate;
    if (data.complete) {
      [completed addObject:view];
    } else {
      [items addObject:view];
    }
  }
  // Completed items appear at the end of the list.
  [items addObjectsFromArray:completed];
  return items;
}

@end
