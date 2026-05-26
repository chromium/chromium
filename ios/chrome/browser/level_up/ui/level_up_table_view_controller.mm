// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_table_view_controller.h"

#import "ios/chrome/browser/level_up/ui/level_up_task.h"
#import "ios/chrome/browser/level_up/ui/level_up_task_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The alpha opacity of row separators.
const CGFloat kSeparatorAlpha = 0.4;
// The card corner radius.
const CGFloat kCardCornerRadius = 24.0;
// Padding spacing.
const CGFloat kLayoutSpacing = 16.0;
// The fixed height for each task row cell.
const CGFloat kCellRowHeight = 75.0;
// The reuse identifier for LevelUpTaskCell rows.
NSString* const kCellIdentifier = @"LevelUpTaskCellIdentifier";

}  // namespace

@implementation LevelUpTableViewController {
  // The list of tasks.
  NSArray<LevelUpTask*>* _tasks;
  // The section header view.
  UIView* _headerView;
  // The card header title text.
  NSString* _headerTitle;
  // Whether to show the "See All" button in the card header.
  BOOL _showsSeeAllButton;
  // The height constraint.
  NSLayoutConstraint* _heightConstraint;
}

- (instancetype)initWithHeaderTitle:(NSString*)headerTitle
                  showsSeeAllButton:(BOOL)showsSeeAllButton
                              style:(UITableViewStyle)style {
  self = [super initWithStyle:style];
  if (self) {
    _headerTitle = headerTitle;
    _showsSeeAllButton = showsSeeAllButton;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.translatesAutoresizingMaskIntoConstraints = NO;
  self.tableView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.tableView.separatorColor = [[UIColor colorNamed:kSeparatorColor]
      colorWithAlphaComponent:kSeparatorAlpha];
  self.tableView.scrollEnabled = YES;
  self.tableView.sectionHeaderTopPadding = 0.0;

  self.tableView.layer.cornerRadius = kCardCornerRadius;
  self.tableView.layer.masksToBounds = YES;

  self.tableView.rowHeight = kCellRowHeight;
  [self.tableView registerClass:[LevelUpTaskCell class]
         forCellReuseIdentifier:kCellIdentifier];

  _heightConstraint =
      [self.tableView.heightAnchor constraintEqualToConstant:0.0];
  _heightConstraint.active = YES;

  [self setupSectionHeaderView];
}

#pragma mark - LevelUpConsumer

- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks {
  _tasks = [tasks copy];
  [self.tableView reloadData];

  [self.tableView layoutIfNeeded];
  NSInteger taskCount = [self tableView:self.tableView numberOfRowsInSection:0];
  CGFloat headerHeight = [self.tableView rectForHeaderInSection:0].size.height;
  CGFloat rowHeight = self.tableView.rowHeight;
  _heightConstraint.constant = headerHeight + (taskCount * rowHeight);
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _tasks.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  LevelUpTask* task = _tasks[indexPath.row];
  LevelUpTaskCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kCellIdentifier
                                      forIndexPath:indexPath];
  [cell configureWithTask:task];
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  LevelUpTask* task = _tasks[indexPath.row];
  if (task.navigationAction) {
    task.navigationAction();
  }
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  return _headerView;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return UITableViewAutomaticDimension;
}

#pragma mark - Private

// Configures and assigns the table view's sticky section header.
- (void)setupSectionHeaderView {
  UIView* headerRow = [self createHeaderRow];
  UIView* headerSeparator = [self createHeaderSeparator];

  _headerView = [[UIView alloc] init];
  _headerView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  [_headerView addSubview:headerRow];
  [_headerView addSubview:headerSeparator];

  [self setupHeaderConstraintsWithHeaderRow:headerRow
                            headerSeparator:headerSeparator];
}

// Sets up Auto Layout constraints for elements inside the section header.
- (void)setupHeaderConstraintsWithHeaderRow:(UIView*)headerRow
                            headerSeparator:(UIView*)headerSeparator {
  AddSameConstraintsToSidesWithInsets(
      headerRow, _headerView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing, 0,
                                  kLayoutSpacing));
  AddSameConstraintsToSides(headerSeparator, _headerView,
                            LayoutSides::kLeading | LayoutSides::kTrailing);

  [NSLayoutConstraint activateConstraints:@[
    [headerSeparator.topAnchor constraintEqualToAnchor:headerRow.bottomAnchor
                                              constant:kLayoutSpacing],
    [headerSeparator.bottomAnchor
        constraintEqualToAnchor:_headerView.bottomAnchor],
  ]];
}

// Creates the tasks card header row view.
- (UIView*)createHeaderRow {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  UIFont* subheadFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  UIFontDescriptor* titleBoldDescriptor = [subheadFont.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  titleLabel.font = [UIFont fontWithDescriptor:titleBoldDescriptor size:0.0];
  titleLabel.text = _headerTitle;

  UIButton* seeAllButton = [UIButton buttonWithType:UIButtonTypeSystem];
  seeAllButton.translatesAutoresizingMaskIntoConstraints = NO;
  [seeAllButton setTitle:l10n_util::GetNSString(IDS_IOS_LEVEL_UP_SEE_ALL)
                forState:UIControlStateNormal];
  [seeAllButton setTitleColor:[UIColor colorNamed:kBlueColor]
                     forState:UIControlStateNormal];

  UIFontDescriptor* seeAllBoldDescriptor = [subheadFont.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  seeAllButton.titleLabel.font = [UIFont fontWithDescriptor:seeAllBoldDescriptor
                                                       size:0.0];
  seeAllButton.hidden = !_showsSeeAllButton;

  [seeAllButton addTarget:self
                   action:@selector(didTapSeeAll)
         forControlEvents:UIControlEventTouchUpInside];

  UIView* spacer = [[UIView alloc] init];
  [spacer setContentHuggingPriority:UILayoutPriorityDefaultLow
                            forAxis:UILayoutConstraintAxisHorizontal];

  UIStackView* headerStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, spacer, seeAllButton ]];
  headerStack.axis = UILayoutConstraintAxisHorizontal;
  headerStack.alignment = UIStackViewAlignmentCenter;
  headerStack.translatesAutoresizingMaskIntoConstraints = NO;

  return headerStack;
}

// Creates the card header separator line.
- (UIView*)createHeaderSeparator {
  UIView* headerSeparator = [[UIView alloc] init];
  headerSeparator.translatesAutoresizingMaskIntoConstraints = NO;
  headerSeparator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [headerSeparator.heightAnchor constraintEqualToConstant:1.0].active = YES;
  return headerSeparator;
}

// Target helper executed when user taps the "See All" action link.
- (void)didTapSeeAll {
  [self.delegate didTapSeeAllTasks:self];
}

@end
