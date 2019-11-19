// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_base_view_controller+internal.h"

#import "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxPopupViewController () <OmniboxPopupRowCellDelegate>
@end

@implementation OmniboxPopupViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = kOmniboxPopupCellMinimumHeight;

  [self.tableView registerClass:[OmniboxPopupRowCell class]
         forCellReuseIdentifier:OmniboxPopupRowCellReuseIdentifier];
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  [super setSemanticContentAttribute:semanticContentAttribute];

  // If there are any visible cells, update them right away.
  for (UITableViewCell* cell in self.tableView.visibleCells) {
    if ([cell isKindOfClass:[OmniboxPopupRowCell class]]) {
      OmniboxPopupRowCell* rowCell =
          base::mac::ObjCCastStrict<OmniboxPopupRowCell>(cell);
      // This has to be set here because the cell's content view has its
      // semantic content attribute reset before the cell is displayed (and
      // before this method is called).
      rowCell.omniboxSemanticContentAttribute = self.semanticContentAttribute;
    }
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];

  // TODO(crbug.com/733650): Default to the dragging check once it's been tested
  // on trunk.
  if (!scrollView.dragging)
    return;

  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView willDisplayCell:cell forRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[OmniboxPopupRowCell class]]) {
    OmniboxPopupRowCell* rowCell =
        base::mac::ObjCCastStrict<OmniboxPopupRowCell>(cell);
    // This has to be set here because the cell's content view has its
    // semantic content attribute reset before the cell is displayed (and before
    // this method is called).
    rowCell.omniboxSemanticContentAttribute = self.semanticContentAttribute;
  }
}

#pragma mark - Table view data source

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.shortcutsEnabled && indexPath.row == 0 &&
      self.currentResult.count == 0) {
    return self.shortcutsViewController.collectionView.collectionViewLayout
        .collectionViewContentSize.height;
  }
  return UITableViewAutomaticDimension;
}

// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);

  if (self.shortcutsEnabled && indexPath.row == 0 &&
      self.currentResult.count == 0) {
    return self.shortcutsCell;
  }

  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  OmniboxPopupRowCell* cell = [self.tableView
      dequeueReusableCellWithIdentifier:OmniboxPopupRowCellReuseIdentifier
                           forIndexPath:indexPath];
  cell.faviconRetriever = self.faviconRetriever;
  cell.imageRetriever = self.imageRetriever;
  [cell setupWithAutocompleteSuggestion:self.currentResult[indexPath.row]
                              incognito:self.incognito];
  cell.showsSeparator =
      (NSUInteger)indexPath.row < self.currentResult.count - 1;
  cell.delegate = self;

  return cell;
}

#pragma mark - OmniboxPopupRowCellDelegate

- (void)trailingButtonTappedForCell:(OmniboxPopupRowCell*)cell {
  NSIndexPath* indexPath = [self.tableView indexPathForCell:cell];
  [self.delegate autocompleteResultConsumer:self
                 didTapTrailingButtonForRow:indexPath.row];
}

- (void)updateTableViewWithAnimation:(BOOL)animation {
  [self.tableView reloadData];
}

@end
