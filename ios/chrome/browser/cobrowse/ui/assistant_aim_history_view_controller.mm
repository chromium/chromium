// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The reuse identifier for the history table view cell.
NSString* const kHistoryCellIdentifier = @"AssistantAIMHistoryCell";

// The estimated row height for the history table view cells.
const CGFloat kTableViewEstimatedRowHeight = 56.0;

// The top inset for the history table view.
const CGFloat kTableViewTopInset = 8.0;

// The number of lines for the title label in the history cell.
const NSInteger kTitleNumberOfLines = 2;

}  // namespace

@interface AssistantAIMHistoryViewController () <UITableViewDataSource,
                                                 UITableViewDelegate>
@end

@implementation AssistantAIMHistoryViewController {
  UITableView* _tableView;
  std::vector<AssistantAIMHistoryItem> _items;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  [self setUpTableView];
}

- (void)setUpTableView {
  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStyleInsetGrouped];
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.delegate = self;
  _tableView.dataSource = self;
  _tableView.rowHeight = UITableViewAutomaticDimension;
  _tableView.estimatedRowHeight = kTableViewEstimatedRowHeight;
  _tableView.separatorStyle = UITableViewCellSeparatorStyleSingleLine;
  _tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  [_tableView registerClass:[UITableViewCell class]
      forCellReuseIdentifier:kHistoryCellIdentifier];

  [self.view addSubview:_tableView];

  LayoutSides sides = LayoutSides::kTop | LayoutSides::kLeading |
                      LayoutSides::kTrailing | LayoutSides::kBottom;
  NSDirectionalEdgeInsets insets =
      NSDirectionalEdgeInsetsMake(kTableViewTopInset, 0, 0, 0);
  AddSameConstraintsToSidesWithInsets(_tableView, self.view, sides, insets);
}

- (void)updateHistoryItems:(const std::vector<AssistantAIMHistoryItem>&)items {
  _items = items;
  [_tableView reloadData];
}

#pragma mark - Actions

- (void)didTapDismiss {
  [self.delegate assistantAIMHistoryViewControllerDidTapDismiss:self];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _items.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kHistoryCellIdentifier
                                      forIndexPath:indexPath];
  const AssistantAIMHistoryItem& item = _items[indexPath.row];

  UIListContentConfiguration* content = [cell defaultContentConfiguration];
  content.text = base::SysUTF8ToNSString(item.title);
  content.textProperties.numberOfLines = kTitleNumberOfLines;
  content.textProperties.lineBreakMode = NSLineBreakByTruncatingTail;
  content.textProperties.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  cell.contentConfiguration = content;

  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;

  return cell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  const AssistantAIMHistoryItem& item = _items[indexPath.row];
  [self.delegate
      assistantAIMHistoryViewController:self
                    didSelectTaskWithId:base::SysUTF8ToNSString(item.task_id)];
}

@end
