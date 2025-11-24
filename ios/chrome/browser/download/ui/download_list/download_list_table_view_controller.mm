// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_action_delegate.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_group_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_grouping_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_mutator.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_cell_content_configuration.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_header.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_illustrated_empty_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
/// Constants for cancel button styling.
static const CGFloat kCancelButtonIconSize = 30;

/// Constants for timer update intervals.
static constexpr base::TimeDelta kNormalUpdateInterval =
    base::Milliseconds(100);  // 10 FPS
/// Skip ratio for tracking mode: skip 2 out of 3 updates (keep every 3rd
/// update)
static const NSInteger kTrackingModeSkipRatio = 3;

NSString* const kCancelButtonPrimaryActionIdentifier =
    @"kCancelButtonPrimaryActionIdentifier";

// Helper function to create the attributed string with a link.
NSAttributedString* GetAttributedString(NSString* message) {
  NSDictionary* textAttributes =
      [TableViewIllustratedEmptyView defaultTextAttributesForSubtitle];
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
    NSLinkAttributeName : GetFilesAppUrl().absoluteString,
  };

  return AttributedStringFromStringWithLink(message, textAttributes,
                                            linkAttributes);
}

}  // namespace

// Diffable data source types using DownloadListGroupItem for section
// identifiers.
typedef UITableViewDiffableDataSource<DownloadListGroupItem*, DownloadListItem*>
    DownloadListDiffableDataSource;
typedef NSDiffableDataSourceSnapshot<DownloadListGroupItem*, DownloadListItem*>
    DownloadListSnapshot;

@interface DownloadListTableViewController () <
    TableViewIllustratedEmptyViewDelegate>
// Filter header view for download list.
@property(nonatomic, strong) DownloadListTableViewHeader* filterHeaderView;
// Counter to track number of updates for throttling logic.
@property(nonatomic, assign) NSInteger updateCounter;
@end

@implementation DownloadListTableViewController {
  DownloadListDiffableDataSource* _diffableDataSource;
  // Cached download items for timer updates.
  NSArray<DownloadListItem*>* _cachedDownloadItems;
  // Repeating timer for periodic UI updates.
  base::RepeatingTimer _updateTimer;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_LIST_TITLE);

  // Configure navigation bar.
  self.navigationController.navigationBar.prefersLargeTitles = YES;
  UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(closeButtonTapped)];
  self.navigationItem.rightBarButtonItem = closeButton;

  // Configure table view.
  // Register cells for both the base configuration and download-specific
  // configuration.
  [DownloadListTableViewCellContentConfiguration
      registerCellForTableView:self.tableView];
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);

  // Setup filter header view.
  [self setupFilterHeaderView];

  [self configureDiffableDataSource];

  // Load download records.
  [self.mutator loadDownloadRecords];

  // Start periodic updates.
  [self startPeriodicUpdates];
}

#pragma mark - Private

- (void)setupFilterHeaderView {
  self.filterHeaderView = [[DownloadListTableViewHeader alloc] init];
  self.filterHeaderView.mutator = self.mutator;
  [self updateTableHeaderViewFrame];
}

- (void)updateTableHeaderViewFrame {
  if (!self.filterHeaderView) {
    return;
  }

  [self.filterHeaderView setNeedsLayout];
  [self.filterHeaderView layoutIfNeeded];

  CGFloat width = self.tableView.bounds.size.width;
  CGSize fittingSize = [self.filterHeaderView
      systemLayoutSizeFittingSize:CGSizeMake(
                                      width,
                                      UILayoutFittingCompressedSize.height)];

  CGRect newFrame = CGRectMake(0, 0, width, fittingSize.height);
  if (!CGRectEqualToRect(self.filterHeaderView.frame, newFrame)) {
    self.filterHeaderView.frame = newFrame;
    // Reassign to trigger table view layout update.
    if (self.tableView.tableHeaderView == self.filterHeaderView) {
      self.tableView.tableHeaderView = self.filterHeaderView;
    }
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateTableHeaderViewFrame];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self pausePeriodicUpdates];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self resumePeriodicUpdates];
}

- (void)dealloc {
  [self stopPeriodicUpdates];
}

#pragma mark - Periodic Updates

// MARK: - UI Refresh Rate Optimization
// The following methods implement a throttling mechanism to avoid excessive UI
// updates using base::RepeatingTimer. Background: applySnapshot operations take
// approximately 5ms each, so frequent calls can cause performance issues and UI
// stuttering. This system batches updates and controls refresh frequency to
// maintain smooth scrolling and responsive UI. The timer runs on the same
// sequence and implements skip logic for UITrackingRunLoopMode.

/// Starts the periodic update timer.
- (void)startPeriodicUpdates {
  [self stopPeriodicUpdates];

  // Reset update counter
  self.updateCounter = 0;

  // Start base::RepeatingTimer: 100ms intervals (10 FPS) for UI updates.
  // The timer runs on the same sequence (main thread) and we implement
  // frequency reduction logic in performPeriodicUpdate based on run loop mode.
  __weak __typeof(self) weakSelf = self;
  _updateTimer.Start(FROM_HERE, kNormalUpdateInterval, base::BindRepeating(^{
                       [weakSelf performPeriodicUpdate];
                     }));
}

/// Stops the periodic update timer.
- (void)stopPeriodicUpdates {
  _updateTimer.Stop();
}

/// Pauses periodic updates without stopping the timer.
- (void)pausePeriodicUpdates {
  _updateTimer.Stop();
}

- (void)resumePeriodicUpdates {
  if (!_updateTimer.IsRunning()) {
    [self startPeriodicUpdates];
  }
}

/// Performs periodic UI update based on cached download items.
- (void)performPeriodicUpdate {
  // Only update if we have cached items waiting to be processed AND
  // the timer is running (updates are not paused).
  // This prevents unnecessary applySnapshot calls when no data changes
  // occurred or when updates are intentionally paused.
  if (!_cachedDownloadItems || !_updateTimer.IsRunning()) {
    return;
  }

  // Increment update counter for tracking mode throttling.
  self.updateCounter++;

  // Check current runloop mode to determine if we should throttle updates.
  NSString* currentMode = [[NSRunLoop currentRunLoop] currentMode];
  BOOL isInTrackingMode = [currentMode isEqualToString:UITrackingRunLoopMode];

  if (isInTrackingMode) {
    // During UITrackingRunLoopMode (user scrolling), throttle updates by only
    // processing every kTrackingModeSkipRatio-th update to maintain smooth
    // scrolling performance.
    if (self.updateCounter % kTrackingModeSkipRatio != 0) {
      return;
    }
  }

  // Perform throttled UI update based on cached items.
  // This ensures applySnapshot is called at controlled intervals rather than
  // immediately upon each data change, reducing the ~5ms overhead per update.
  [self updateUI];
}

/// Updates the UI with the current cached download items.
- (void)updateUI {
  // Create a new snapshot with date-grouped sections.
  DownloadListSnapshot* snapshot = [[DownloadListSnapshot alloc] init];

  // Group items by date.
  NSArray<DownloadListGroupItem*>* groupItems =
      download_list_grouping_util::GroupDownloadItemsByDate(
          _cachedDownloadItems);

  // Add sections and items.
  for (DownloadListGroupItem* groupItem in groupItems) {
    [snapshot appendSectionsWithIdentifiers:@[ groupItem ]];
    [snapshot appendItemsWithIdentifiers:groupItem.items
               intoSectionWithIdentifier:groupItem];
  }

  // Detect changes between old and new items for efficient updates.
  // This minimizes the work done by applySnapshot by only reconfiguring changed
  // cells rather than rebuilding the entire table view structure.
  NSArray<DownloadListItem*>* oldItems =
      _diffableDataSource.snapshot.itemIdentifiers;
  if (oldItems.count > 0) {
    NSSet<DownloadListItem*>* oldItemsSet = [NSSet setWithArray:oldItems];
    NSMutableArray<DownloadListItem*>* changedItems =
        [[NSMutableArray alloc] init];

    for (DownloadListItem* newItem in _cachedDownloadItems) {
      DownloadListItem* existingItem = [oldItemsSet member:newItem];
      if (existingItem && ![existingItem isEqualToItem:newItem]) {
        [changedItems addObject:newItem];
      }
    }

    if (changedItems.count > 0) {
      [snapshot reconfigureItemsWithIdentifiers:changedItems];
    }
  }

  // Apply the snapshot to update the table view.
  // Note: This operation takes ~5ms, which is why we throttle these calls
  // using the CADisplayLink mechanism above to prevent performance issues.
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];

  // Clear cached items after processing to indicate no pending updates.
  _cachedDownloadItems = nil;
}

#pragma mark - Mutator setter override

- (void)setMutator:(id<DownloadListMutator>)mutator {
  _mutator = mutator;
  // Update the filter view's mutator when it's set.
  if (self.filterHeaderView) {
    self.filterHeaderView.mutator = mutator;
  }
}

/// Dismisses the view controller when the close button is tapped.
- (void)closeButtonTapped {
  [self.downloadListHandler hideDownloadList];
}

/// Configures the diffable data source for the table view.
- (void)configureDiffableDataSource {
  __weak __typeof(self) weakSelf = self;
  _diffableDataSource = [[DownloadListDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          DownloadListItem* item) {
             return [weakSelf cellForItem:item atIndexPath:indexPath];
           }];
  self.tableView.dataSource = _diffableDataSource;
}

/// Creates and configures a cell for the given item at the specified index
/// path.
- (UITableViewCell*)cellForItem:(DownloadListItem*)item
                    atIndexPath:(NSIndexPath*)indexPath {
  // Use the new DownloadListTableViewCellContentConfiguration
  DownloadListTableViewCellContentConfiguration* configuration =
      [DownloadListTableViewCellContentConfiguration
          configurationWithDownloadListItem:item];

  UITableViewCell* cell = [DownloadListTableViewCellContentConfiguration
      dequeueTableViewCell:self.tableView];
  cell.contentConfiguration = configuration;
  [self configureCancelButtonForCell:cell
                       configuration:configuration
                                item:item];
  return cell;
}

/// Configures the cancel button accessory for the given cell and item.
- (void)configureCancelButtonForCell:(UITableViewCell*)cell
                       configuration:
                           (DownloadListTableViewCellContentConfiguration*)
                               configuration
                                item:(DownloadListItem*)item {
  if (configuration.showCancelButton) {
    UIButton* cancelButton =
        base::apple::ObjCCast<UIButton>(cell.accessoryView);
    if (!cancelButton) {
      // Create and configure cancel button if it does not exist.
      cancelButton =
          [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary];
      cancelButton.translatesAutoresizingMaskIntoConstraints = YES;
      UIImage* cancelButtonImage =
          SymbolWithPalette(DefaultSymbolWithPointSize(kXMarkCircleFillSymbol,
                                                       kCancelButtonIconSize),
                            @[
                              [UIColor colorNamed:kGrey600Color],
                              [UIColor colorNamed:kGrey200Color],
                            ]);
      [cancelButton setImage:cancelButtonImage forState:UIControlStateNormal];
      cancelButton.frame =
          CGRectMake(0, 0, kCancelButtonIconSize, kCancelButtonIconSize);
      cancelButton.accessibilityLabel = l10n_util::GetNSString(
          IDS_IOS_DOWNLOAD_LIST_CANCEL_ACCESSIBILITY_LABEL);
      cell.accessoryView = cancelButton;
    }
    __weak __typeof(self) weakSelf = self;
    UIAction* primaryAction =
        [UIAction actionWithTitle:@""
                            image:nil
                       identifier:kCancelButtonPrimaryActionIdentifier
                          handler:^(__kindof UIAction* action) {
                            [weakSelf.mutator cancelDownloadItem:item];
                          }];
    [cancelButton addAction:primaryAction
           forControlEvents:UIControlEventTouchUpInside];
  } else {
    cell.accessoryView = nil;
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  DownloadListItem* item =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];

  CHECK(item);
  if (item.downloadState != web::DownloadTask::State::kComplete) {
    return;
  }
  if (item.filePath.empty()) {
    return;
  }
  base::FilePath filePath = item.filePath;
  std::string mimeType = base::SysNSStringToUTF8(item.mimeType);

  [self.downloadRecordHandler openFileWithPath:filePath mimeType:mimeType];
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  DownloadListItem* item =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];

  // Downloads with no available actions do not support context menu.
  if (item.availableActions == DownloadListItemActionNone) {
    return nil;
  }

  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        return [weakSelf createMenuForDownloadItem:item];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (UIMenu*)createMenuForDownloadItem:(DownloadListItem*)item {
  NSMutableArray<UIMenuElement*>* actions = [[NSMutableArray alloc] init];
  __weak __typeof(self) weakSelf = self;
  DownloadListItemAction availableActions = item.availableActions;

  // Check if "Open in Files App" action is available.
  if (availableActions & DownloadListItemActionOpenInFiles) {
    UIAction* openInFilesAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(
                            IDS_IOS_OPEN_IN_FILES_APP_ACTION_TITLE)
                  image:DefaultSymbolWithPointSize(kOpenImageActionSymbol,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf.actionDelegate openDownloadInFiles:item];
                }];
    [actions addObject:openInFilesAction];
  }

  // Check if Delete action is available.
  if (availableActions & DownloadListItemActionDelete) {
    UIAction* deleteAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)
                  image:DefaultSymbolWithPointSize(kTrashSymbol,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf.mutator deleteDownloadItem:item];
                }];
    deleteAction.attributes = UIMenuElementAttributesDestructive;
    [actions addObject:deleteAction];
  }

  return [UIMenu menuWithTitle:@"" children:actions];
}

- (UISwipeActionsConfiguration*)tableView:(UITableView*)tableView
    trailingSwipeActionsConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath {
  DownloadListItem* item =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];

  // Only show delete action if it's available for this item.
  if (!item || !(item.availableActions & DownloadListItemActionDelete)) {
    return nil;
  }

  __weak __typeof(self) weakSelf = self;

  // Create delete action.
  UIContextualAction* deleteAction = [UIContextualAction
      contextualActionWithStyle:UIContextualActionStyleDestructive
                          title:nil
                        handler:^(UIContextualAction* action,
                                  __kindof UIView* sourceView,
                                  void (^completionHandler)(BOOL)) {
                          [weakSelf.mutator deleteDownloadItem:item];
                          completionHandler(YES);
                        }];

  // Set delete action icon.
  deleteAction.image =
      DefaultSymbolWithPointSize(kTrashSymbol, kSymbolActionPointSize);

  return
      [UISwipeActionsConfiguration configurationWithActions:@[ deleteAction ]];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  // Get the section identifier (DownloadListGroupItem) from the data source
  // snapshot.
  DownloadListGroupItem* groupItem =
      [_diffableDataSource sectionIdentifierForIndex:section];

  TableViewTextHeaderFooterView* headerView =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(tableView);
  [headerView setTitle:groupItem.title];

  return headerView;
}

#pragma mark - DownloadListConsumer

- (void)setDownloadListItems:(NSArray<DownloadListItem*>*)items {
  // Cache the input parameters for throttled DisplayLink updates
  // Instead of immediately calling applySnapshot (which costs ~5ms), we cache
  // the new items and let the CADisplayLink timer handle the actual UI update
  // at controlled intervals. This prevents excessive refresh calls that could
  // cause UI stuttering, especially during rapid data changes.
  _cachedDownloadItems = [items copy];
}

- (void)setLoadingState:(BOOL)loading {
}

- (void)setEmptyState:(BOOL)empty {
  if (empty) {
    // Empty downloads: show small title and empty view.
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
    if (!self.tableView.backgroundView) {
      UIImage* emptyImage = [UIImage imageNamed:@"download_list_empty"];
      TableViewIllustratedEmptyView* emptyView = [[TableViewIllustratedEmptyView
          alloc] initWithFrame:self.view.bounds
                         image:emptyImage
                         title:l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_LIST_NO_ENTRIES_TITLE)
            attributedSubtitle:GetAttributedString(l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_LIST_NO_ENTRIES_MESSAGE))];
      emptyView.delegate = self;
      self.tableView.backgroundView = emptyView;
    }
  } else {
    // Non-empty downloads: show large title initially and hide empty view.
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeAlways;
    self.tableView.backgroundView = nil;
  }
  if (self.filterHeaderView && self.filterHeaderView.isHidden == NO) {
    [self.filterHeaderView setAttributionTextShown:!empty];
  }
}

- (void)setDownloadListHeaderShown:(BOOL)shown {
  if (shown) {
    // Show the filter view if it's not already set.
    if (self.tableView.tableHeaderView != self.filterHeaderView) {
      self.tableView.tableHeaderView = self.filterHeaderView;
    }
  } else {
    // Hide the filter view by setting tableHeaderView to nil.
    self.tableView.tableHeaderView = nil;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

/// Called before the presentation controller will dismiss.
- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  [self.downloadListHandler hideDownloadList];
}
#pragma mark - TableViewIllustratedEmptyViewDelegate

// Invoked when a link in `view`'s subtitle is tapped.
- (void)tableViewIllustratedEmptyView:(TableViewIllustratedEmptyView*)view
                   didTapSubtitleLink:(NSURL*)URL {
  if (!URL) {
    return;
  }
  [[UIApplication sharedApplication] openURL:URL
                                     options:@{}
                           completionHandler:nil];
}

@end
