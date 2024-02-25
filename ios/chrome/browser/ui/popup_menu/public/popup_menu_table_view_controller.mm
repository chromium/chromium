// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_metrics_handler.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_footer_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_ui_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {
const CGFloat kFooterHeight = 21;
const CGFloat kPopupMenuVerticalInsets = 7;
const CGFloat kScrollIndicatorVerticalInsets = 11;
}  // namespace

@interface PopupMenuTableViewController ()
// Whether the -viewDidAppear: callback has been called.
@property(nonatomic, assign) BOOL viewDidAppear;
// A cached copy of `self.view.bounds`, made during calls to
// viewDidLayoutSubviews, and used to reduce the number of calls to
// calculatePreferredContentSize. which can be an expensive operation.
@property(nonatomic, assign) CGRect cachedBounds;
// Tracks reusable cells in memory, which has an upper limit. This is used to
// ensure that pointer interaction is added only once to a cell.
@property(nonatomic, strong) NSHashTable<UITableViewCell*>* cellsInMemory;
@end

@implementation PopupMenuTableViewController

@dynamic tableViewModel;
@synthesize baseViewController = _baseViewController;
@synthesize delegate = _delegate;
@synthesize itemToHighlight = _itemToHighlight;
@synthesize viewDidAppear = _viewDidAppear;

- (instancetype)init {
  self = [super initWithStyle:UITableViewStyleGrouped];
  if (self) {
    self.cellsInMemory = [NSHashTable<UITableViewCell*> weakObjectsHashTable];
    self.cachedBounds = CGRectZero;
  }
  return self;
}

#pragma mark - PopupMenuConsumer

- (void)setItemToHighlight:(TableViewItem<PopupMenuItem>*)itemToHighlight {
  DCHECK_GT(self.tableViewModel.numberOfSections, 0L);
  _itemToHighlight = itemToHighlight;
  if (itemToHighlight && self.viewDidAppear) {
    [self highlightItem:itemToHighlight repeat:YES];
  }
}

- (void)setPopupMenuItems:
    (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)items {
  [super loadModel];
  for (NSUInteger section = 0; section < items.count; section++) {
    NSInteger sectionIdentifier = kSectionIdentifierEnumZero + section;
    [self.tableViewModel addSectionWithIdentifier:sectionIdentifier];
    for (TableViewItem<PopupMenuItem>* item in items[section]) {
      [self.tableViewModel addItem:item
           toSectionWithIdentifier:sectionIdentifier];
    }

    if (section != items.count - 1) {
      // Add a footer for all sections except the last one.
      TableViewHeaderFooterItem* footer =
          [[PopupMenuFooterItem alloc] initWithType:kItemTypeEnumZero];
      [self.tableViewModel setFooter:footer
            forSectionWithIdentifier:sectionIdentifier];
    }
  }
  [self.tableView reloadData];
  self.preferredContentSize = [self calculatePreferredContentSize];
}

- (void)itemsHaveChanged:(NSArray<TableViewItem<PopupMenuItem>*>*)items {
  [self reconfigureCellsForItems:items];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.styler.tableViewBackgroundColor = nil;
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  [super viewDidLoad];
  self.tableView.contentInset = UIEdgeInsetsMake(kPopupMenuVerticalInsets, 0,
                                                 kPopupMenuVerticalInsets, 0);
  self.tableView.scrollIndicatorInsets = UIEdgeInsetsMake(
      kScrollIndicatorVerticalInsets, 0, kScrollIndicatorVerticalInsets, 0);
  self.tableView.rowHeight = 0;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  // Adding a tableHeaderView is needed to prevent a wide inset on top of the
  // collection.
  self.tableView.tableHeaderView = [[UIView alloc]
      initWithFrame:CGRectMake(0.0f, 0.0f, self.tableView.bounds.size.width,
                               0.01f)];

  self.view.layer.cornerRadius = kPopupMenuCornerRadius;
  self.view.layer.masksToBounds = YES;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.viewDidAppear = YES;
  if (self.itemToHighlight) {
    [self highlightItem:self.itemToHighlight repeat:YES];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // -calculatePreferredContentSize is a somewhat expensive operation and may
  // be responsible for some system hangs. Optimize away some calls by checking
  // if bounds changed first. See crbug.com/1257151 for more context.
  if (!CGRectEqualToRect(self.cachedBounds, self.view.bounds)) {
    self.preferredContentSize = [self calculatePreferredContentSize];
    self.cachedBounds = self.view.bounds;
  }
}

- (CGSize)calculatePreferredContentSize {
  CGFloat width = 0;
  CGFloat height = 0;
  for (NSInteger section = 0; section < [self.tableViewModel numberOfSections];
       section++) {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSectionIndex:section];
    for (TableViewItem<PopupMenuItem>* item in
         [self.tableViewModel itemsInSectionWithIdentifier:sectionIdentifier]) {
      CGSize sizeForCell = [item cellSizeForWidth:self.view.bounds.size.width];
      width = MAX(width, ceil(sizeForCell.width));
      height += sizeForCell.height;
    }
    // Add the separator height (only available the non-final sections).
    height += [self tableView:self.tableView heightForFooterInSection:section];
  }
  height +=
      self.tableView.contentInset.top + self.tableView.contentInset.bottom;
  return CGSizeMake(width, ceil(height));
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.metricsHandler popupMenuScrolledVertically];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.metricsHandler popupMenuTookAction];
  UIView* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  CGPoint center = [cell convertPoint:cell.center toView:nil];
  [self.delegate popupMenuTableViewController:self
                                didSelectItem:[self.tableViewModel
                                                  itemAtIndexPath:indexPath]
                                       origin:center];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if (section == self.tableViewModel.numberOfSections - 1)
    return 0;
  return kFooterHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem<PopupMenuItem>* item =
      [self.tableViewModel itemAtIndexPath:indexPath];
  return [item cellSizeForWidth:self.view.bounds.size.width].height;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if (![self.cellsInMemory containsObject:cell]) {
    [cell addInteraction:[[ViewPointerInteraction alloc] init]];
    [self.cellsInMemory addObject:cell];
  }
  return cell;
}

#pragma mark - Private

// Returns the index path identifying the the row at the position `point`.
// `point` must be in the window coordinates. Returns nil if `point` is outside
// the bounds of the table view.
- (NSIndexPath*)indexPathForInnerRowAtPoint:(CGPoint)point {
  CGPoint pointInTableViewCoordinates = [self.tableView convertPoint:point
                                                            fromView:nil];
  CGRect insetRect =
      CGRectInset(self.tableView.bounds, 0, kPopupMenuVerticalInsets);
  BOOL pointInTableViewBounds =
      CGRectContainsPoint(insetRect, pointInTableViewCoordinates);

  NSIndexPath* indexPath = nil;
  if (pointInTableViewBounds) {
    indexPath =
        [self.tableView indexPathForRowAtPoint:pointInTableViewCoordinates];
  }

  return indexPath;
}

// Highlights the `item` and `repeat` the highlighting once.
- (void)highlightItem:(TableViewItem<PopupMenuItem>*)item repeat:(BOOL)repeat {
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  [self.tableView selectRowAtIndexPath:indexPath
                              animated:YES
                        scrollPosition:UITableViewScrollPositionNone];
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    (int64_t)(kHighlightAnimationDuration * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [self unhighlightItem:item repeat:repeat];
      });
}

// Removes the highlight from `item` and `repeat` the highlighting once.
- (void)unhighlightItem:(TableViewItem<PopupMenuItem>*)item
                 repeat:(BOOL)repeat {
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  if (!repeat)
    return;

  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    (int64_t)(kHighlightAnimationDuration * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [self highlightItem:item repeat:NO];
      });
}

@end
