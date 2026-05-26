// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_all_tasks_view_controller.h"

#import "ios/chrome/browser/level_up/ui/level_up_table_view_controller.h"
#import "ios/chrome/browser/level_up/ui/level_up_task.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing for layout margins and stacks.
const CGFloat kLayoutSpacing = 16.0;

}  // namespace

@implementation LevelUpAllTasksViewController {
  // The root scroll view.
  UIScrollView* _scrollView;
  // The main vertical container stack view.
  UIStackView* _allTasksContainerView;
  // The list of categories cached before view loaded.
  NSMutableArray<LevelUpCategory*>* _categories;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _categories = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.title = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_SEE_ALL);

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.alwaysBounceVertical = YES;
  [self.view addSubview:_scrollView];
  AddSameConstraints(_scrollView, self.view);

  _allTasksContainerView = [[UIStackView alloc] init];
  _allTasksContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _allTasksContainerView.axis = UILayoutConstraintAxisVertical;
  _allTasksContainerView.spacing = kLayoutSpacing;
  _allTasksContainerView.alignment = UIStackViewAlignmentFill;
  [_scrollView addSubview:_allTasksContainerView];

  AddSameConstraintsWithInsets(
      _allTasksContainerView, _scrollView.contentLayoutGuide,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing,
                                  kLayoutSpacing, kLayoutSpacing));
  [_allTasksContainerView.widthAnchor
      constraintEqualToAnchor:_scrollView.widthAnchor
                     constant:-2 * kLayoutSpacing]
      .active = YES;

  for (LevelUpCategory* category in _categories) {
    [self appendCategoryCard:category];
  }
}

#pragma mark - LevelUpConsumer

- (void)addCategoryCard:(LevelUpCategory*)category {
  [_categories addObject:category];

  if (self.isViewLoaded) {
    [self appendCategoryCard:category];
  }
}

#pragma mark - Private

// Instantiates and appends a category table view card.
- (void)appendCategoryCard:(LevelUpCategory*)category {
  LevelUpTableViewController* categoryVC = [[LevelUpTableViewController alloc]
      initWithHeaderTitle:category.title
        showsSeeAllButton:NO
                    style:UITableViewStylePlain];
  [self addChildViewController:categoryVC];
  [categoryVC didMoveToParentViewController:self];

  [_allTasksContainerView addArrangedSubview:categoryVC.tableView];

  [categoryVC setLevel:0 tasksForLevel:category.tasks];
}

@end
