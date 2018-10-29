// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_footer_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller_commands.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
const CGFloat kFooterHeight = 21;
const CGFloat kPopupMenuVerticalInsets = 7;
const CGFloat kScrollIndicatorVerticalInsets = 11;
}  // namespace

@interface PopupMenuTableViewController ()
// Whether the -viewDidAppear: callback has been called.
@property(nonatomic, assign) BOOL viewDidAppear;
@end

@implementation PopupMenuTableViewController

@dynamic tableViewModel;
@synthesize baseViewController = _baseViewController;
@synthesize commandHandler = _commandHandler;
@synthesize dispatcher = _dispatcher;
@synthesize itemToHighlight = _itemToHighlight;
@synthesize viewDidAppear = _viewDidAppear;

- (instancetype)init {
  return [super initWithTableViewStyle:UITableViewStyleGrouped
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
}

- (void)selectRowAtPoint:(CGPoint)point {
  NSIndexPath* rowIndexPath = [self indexPathForInnerRowAtPoint:point];
  if (!rowIndexPath)
    return;

  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:rowIndexPath];
  if (!cell.userInteractionEnabled)
    return;

  base::RecordAction(base::UserMetricsAction("MobilePopupMenuSwipeToSelect"));
  [self executeActionForItem:[self.tableViewModel itemAtIndexPath:rowIndexPath]
                      origin:[cell convertPoint:cell.center toView:nil]];
}

- (void)focusRowAtPoint:(CGPoint)point {
  NSIndexPath* rowIndexPath = [self indexPathForInnerRowAtPoint:point];

  BOOL rowAlreadySelected = NO;
  NSArray<NSIndexPath*>* selectedRows =
      [self.tableView indexPathsForSelectedRows];
  for (NSIndexPath* selectedIndexPath in selectedRows) {
    if (selectedIndexPath == rowIndexPath) {
      rowAlreadySelected = YES;
      continue;
    }
    [self.tableView deselectRowAtIndexPath:selectedIndexPath animated:NO];
  }

  if (!rowAlreadySelected && rowIndexPath) {
    [self.tableView selectRowAtIndexPath:rowIndexPath
                                animated:NO
                          scrollPosition:UITableViewScrollPositionNone];
    TriggerHapticFeedbackForSelectionChange();
  }
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
}

- (void)itemsHaveChanged:(NSArray<TableViewItem<PopupMenuItem>*>*)items {
  [self reconfigureCellsForItems:items];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.styler.tableViewBackgroundColor = nil;
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

  if (!base::ios::IsRunningOnIOS11OrLater()) {
    // On iOS 10, a footer with a height of 0 is also needed to prevent inset at
    // the bottom.
    self.tableView.tableFooterView = [[UIView alloc]
        initWithFrame:CGRectMake(0.0f, 0.0f, self.tableView.bounds.size.width,
                                 0.01f)];
    self.tableView.sectionFooterHeight = 0.0;
  }

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

- (CGSize)preferredContentSize {
  CGFloat width = 0;
  CGFloat height = 0;
  for (NSInteger section = 0; section < [self.tableViewModel numberOfSections];
       section++) {
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSection:section];
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

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  UIView* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  CGPoint center = [cell convertPoint:cell.center toView:nil];
  [self executeActionForItem:[self.tableViewModel itemAtIndexPath:indexPath]
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

#pragma mark - Private

// Returns the index path identifying the the row at the position |point|.
// |point| must be in the window coordinates. Returns nil if |point| is outside
// the bounds of the table view.
- (NSIndexPath*)indexPathForInnerRowAtPoint:(CGPoint)point {
  CGPoint pointInTableViewCoordinates =
      [self.tableView convertPoint:point fromView:nil];
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

// Highlights the |item| and |repeat| the highlighting once.
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

// Removes the highlight from |item| and |repeat| the highlighting once.
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

// Executes the action associated with |identifier|, using |origin| as the point
// of origin of the action if one is needed.
- (void)executeActionForItem:(TableViewItem<PopupMenuItem>*)item
                      origin:(CGPoint)origin {
  PopupMenuAction identifier = item.actionIdentifier;
  switch (identifier) {
    case PopupMenuActionReload:
      base::RecordAction(UserMetricsAction("MobileMenuReload"));
      [self.dispatcher reload];
      break;
    case PopupMenuActionStop:
      base::RecordAction(UserMetricsAction("MobileMenuStop"));
      [self.dispatcher stopLoading];
      break;
    case PopupMenuActionOpenNewTab:
      base::RecordAction(UserMetricsAction("MobileMenuNewTab"));
      [self.dispatcher
          openURLInNewTab:[OpenNewTabCommand commandWithIncognito:NO
                                                      originPoint:origin]];
      break;
    case PopupMenuActionOpenNewIncognitoTab:
      base::RecordAction(UserMetricsAction("MobileMenuNewIncognitoTab"));
      [self.dispatcher
          openURLInNewTab:[OpenNewTabCommand commandWithIncognito:YES
                                                      originPoint:origin]];
      break;
    case PopupMenuActionReadLater:
      base::RecordAction(UserMetricsAction("MobileMenuReadLater"));
      [self.commandHandler readPageLater];
      break;
    case PopupMenuActionPageBookmark:
      base::RecordAction(UserMetricsAction("MobileMenuAddToBookmarks"));
      [self.dispatcher bookmarkPage];
      break;
    case PopupMenuActionFindInPage:
      base::RecordAction(UserMetricsAction("MobileMenuFindInPage"));
      [self.dispatcher showFindInPage];
      break;
    case PopupMenuActionRequestDesktop:
      base::RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
      [self.dispatcher requestDesktopSite];
      break;
    case PopupMenuActionRequestMobile:
      base::RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
      [self.dispatcher requestMobileSite];
      break;
    case PopupMenuActionSiteInformation:
      base::RecordAction(UserMetricsAction("MobileMenuSiteInformation"));
      [self.dispatcher
          showPageInfoForOriginPoint:self.baseViewController.view.center];
      break;
    case PopupMenuActionReportIssue:
      base::RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
      [self.dispatcher
          showReportAnIssueFromViewController:self.baseViewController];
      // Dismisses the popup menu without animation to allow the snapshot to be
      // taken without the menu presented.
      [self.dispatcher dismissPopupMenuAnimated:NO];
      break;
    case PopupMenuActionHelp:
      base::RecordAction(UserMetricsAction("MobileMenuHelp"));
      [self.dispatcher showHelpPage];
      break;
#if !defined(NDEBUG)
    case PopupMenuActionViewSource:
      [self.dispatcher viewSource];
      break;
#endif  // !defined(NDEBUG)

    case PopupMenuActionBookmarks:
      base::RecordAction(UserMetricsAction("MobileMenuAllBookmarks"));
      [self.dispatcher showBookmarksManager];
      break;
    case PopupMenuActionReadingList:
      base::RecordAction(UserMetricsAction("MobileMenuReadingList"));
      [self.dispatcher showReadingList];
      break;
    case PopupMenuActionRecentTabs:
      base::RecordAction(UserMetricsAction("MobileMenuRecentTabs"));
      [self.dispatcher showRecentTabs];
      break;
    case PopupMenuActionHistory:
      base::RecordAction(UserMetricsAction("MobileMenuHistory"));
      [self.dispatcher showHistory];
      break;
    case PopupMenuActionSettings:
      base::RecordAction(UserMetricsAction("MobileMenuSettings"));
      [self.dispatcher showSettingsFromViewController:self.baseViewController];
      break;
    case PopupMenuActionCloseTab:
      base::RecordAction(UserMetricsAction("MobileMenuCloseTab"));
      [self.dispatcher closeCurrentTab];
      break;
    case PopupMenuActionNavigate:
      // No metrics for this item.
      [self.commandHandler navigateToPageForItem:item];
      break;
    case PopupMenuActionPasteAndGo:
      base::RecordAction(UserMetricsAction("MobileMenuPasteAndGo"));
      [self.dispatcher loadQuery:[UIPasteboard generalPasteboard].string
                     immediately:YES];
      break;
    case PopupMenuActionVoiceSearch:
      base::RecordAction(UserMetricsAction("MobileMenuVoiceSearch"));
      [self.dispatcher startVoiceSearch];
      break;
    case PopupMenuActionQRCodeSearch:
      base::RecordAction(UserMetricsAction("MobileMenuScanQRCode"));
      [self.dispatcher showQRScanner];
      break;
  }

  // Close the tools menu.
  [self.dispatcher dismissPopupMenuAnimated:YES];
}

@end
