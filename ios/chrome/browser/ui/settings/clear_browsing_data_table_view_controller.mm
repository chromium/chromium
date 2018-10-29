// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/elements/chrome_activity_overlay_coordinator.h"
#import "ios/chrome/browser/ui/settings/cells/table_view_clear_browsing_data_item.h"
#include "ios/chrome/browser/ui/settings/clear_browsing_data_local_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_manager.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Separation space between sections.
const CGFloat kSeparationSpaceBetweenSections = 9;
}  // namespace

namespace ios {
class ChromeBrowserState;
}

@interface ClearBrowsingDataTableViewController ()<
    TableViewTextLinkCellDelegate,
    ClearBrowsingDataConsumer>

// TODO(crbug.com/850699): remove direct dependency and replace with
// delegate.
@property(nonatomic, readonly, strong) ClearBrowsingDataManager* dataManager;

// Browser state.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// Coordinator that managers a UIAlertController to clear browsing data.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Coordinator for displaying a modal overlay with native activity indicator to
// prevent the user from interacting with the page.
@property(nonatomic, strong)
    ChromeActivityOverlayCoordinator* chromeActivityOverlayCoordinator;

// Reference to clear browsing data button for positioning popover confirmation
// dialog.
@property(nonatomic, strong) UIButton* clearBrowsingDataButton;

// Modal alert for Browsing history removed dialog.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The data manager might want to reload tableView cells before the tableView
// has loaded, we need to prevent this kind of updates until the tableView
// loads.
@property(nonatomic, assign) BOOL suppressTableViewUpdates;

@end

@implementation ClearBrowsingDataTableViewController
@synthesize actionSheetCoordinator = _actionSheetCoordinator;
@synthesize alertCoordinator = _alertCoordinator;
@synthesize browserState = _browserState;
@synthesize clearBrowsingDataButton = _clearBrowsingDataButton;
@synthesize dataManager = _dataManager;
@synthesize dispatcher = _dispatcher;
@synthesize localDispatcher = _localDispatcher;
@synthesize suppressTableViewUpdates = _suppressTableViewUpdates;

#pragma mark - ViewController Lifecycle.

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _browserState = browserState;
    _dataManager = [[ClearBrowsingDataManager alloc]
        initWithBrowserState:browserState
                    listType:ClearBrowsingDataListType::kListTypeTableView];
    _dataManager.consumer = self;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  // TableView configuration
  self.tableView.estimatedRowHeight = 56;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionHeaderHeight = 0;
  // Add a tableFooterView in order to disable separators at the bottom of the
  // tableView.
  self.tableView.tableFooterView = [[UIView alloc] init];
  self.styler.tableViewBackgroundColor = [UIColor clearColor];
  // Align cell separators with text label leading margin.
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Navigation controller configuration.
  self.title = l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  // Adds the "Done" button and hooks it up to |dismiss|.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismiss)];
  dismissButton.accessibilityIdentifier = kSettingsDoneButtonId;
  self.navigationItem.rightBarButtonItem = dismissButton;

  // Do not allow any TableView updates until the model is fully loaded. The
  // model might try re-loading some cells and the TableView might not be loaded
  // at this point (https://crbug.com/873929).
  self.suppressTableViewUpdates = YES;
  [self loadModel];
  self.suppressTableViewUpdates = NO;
}

- (void)loadModel {
  [super loadModel];
  [self.dataManager loadModel:self.tableViewModel];
}

- (void)dismiss {
  [self prepareForDismissal];
  [self.localDispatcher dismissClearBrowsingDataWithCompletion:nil];
}

#pragma mark - Public Methods

- (void)prepareForDismissal {
  if (self.actionSheetCoordinator) {
    [self.actionSheetCoordinator stop];
    self.actionSheetCoordinator = nil;
  }
  if (self.alertCoordinator) {
    [self.alertCoordinator stop];
    self.alertCoordinator = nil;
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cellToReturn =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeFooterSavedSiteData:
    case ItemTypeFooterClearSyncAndSavedSiteData:
    case ItemTypeFooterGoogleAccountAndMyActivity: {
      TableViewTextLinkCell* tableViewTextLinkCell =
          base::mac::ObjCCastStrict<TableViewTextLinkCell>(cellToReturn);
      [tableViewTextLinkCell setDelegate:self];
      tableViewTextLinkCell.selectionStyle = UITableViewCellSelectionStyleNone;
      // Hide the cell separator inset for footnotes.
      tableViewTextLinkCell.separatorInset =
          UIEdgeInsetsMake(0, tableViewTextLinkCell.bounds.size.width, 0, 0);
      break;
    }
    case ItemTypeClearBrowsingDataButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cellToReturn);
      tableViewTextButtonCell.selectionStyle =
          UITableViewCellSelectionStyleNone;
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(showClearBrowsingDataAlertController:)
          forControlEvents:UIControlEventTouchUpInside];
      self.clearBrowsingDataButton = tableViewTextButtonCell.button;
      break;
    }
    case ItemTypeDataTypeBrowsingHistory:
    case ItemTypeDataTypeCookiesSiteData:
    case ItemTypeDataTypeCache:
    case ItemTypeDataTypeSavedPasswords:
    case ItemTypeDataTypeAutofill:
    default:
      break;
  }
  return cellToReturn;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  switch (item.type) {
    case ItemTypeDataTypeBrowsingHistory:
    case ItemTypeDataTypeCookiesSiteData:
    case ItemTypeDataTypeCache:
    case ItemTypeDataTypeSavedPasswords:
    case ItemTypeDataTypeAutofill: {
      TableViewClearBrowsingDataItem* clearBrowsingDataItem =
          base::mac::ObjCCastStrict<TableViewClearBrowsingDataItem>(item);
      clearBrowsingDataItem.checked = !clearBrowsingDataItem.checked;
      [self reconfigureCellsForItems:@[ clearBrowsingDataItem ]];
      break;
    }
    case ItemTypeClearBrowsingDataButton:
    case ItemTypeFooterGoogleAccount:
    case ItemTypeFooterGoogleAccountAndMyActivity:
    case ItemTypeFooterSavedSiteData:
    case ItemTypeFooterClearSyncAndSavedSiteData:
    case ItemTypeTimeRange:
    default:
      break;
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return kSeparationSpaceBetweenSections;
}

#pragma mark - TableViewTextLinkCellDelegate

- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(const GURL&)URL {
  GURL copiedURL(URL);
  [self.localDispatcher openURL:copiedURL];
}

#pragma mark - ClearBrowsingDataConsumer

- (void)updateCellsForItem:(ListItem*)item {
  if (self.suppressTableViewUpdates)
    return;

  // Reload the item instead of reconfiguring it. This might update
  // TableViewTextLinkItems which which can have different number of lines,
  // thus the cell height needs to adapt accordingly.
  [self reloadCellsForItems:@[ item ]
           withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)removeBrowsingDataForBrowserState:(ios::ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock {
  base::RecordAction(
      base::UserMetricsAction("MobileClearBrowsingDataTriggeredFromUIRefresh"));

  // Show activity indicator modal while removal is happening.
  self.chromeActivityOverlayCoordinator =
      [[ChromeActivityOverlayCoordinator alloc]
          initWithBaseViewController:self.navigationController];
  self.chromeActivityOverlayCoordinator.messageText =
      l10n_util::GetNSStringWithFixup(
          IDS_IOS_CLEAR_BROWSING_DATA_ACTIVITY_MODAL);
  [self.chromeActivityOverlayCoordinator start];

  __weak ClearBrowsingDataTableViewController* weakSelf = self;
  dispatch_time_t timeOneSecondLater =
      dispatch_time(DISPATCH_TIME_NOW, (1 * NSEC_PER_SEC));
  void (^removeBrowsingDidFinishCompletionBlock)(void) = ^void() {
    ClearBrowsingDataTableViewController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    // Sometimes clear browsing data is really short
    // (<1sec), so ensure that overlay displays for at
    // least 1 second instead of looking like a glitch.
    dispatch_after(timeOneSecondLater, dispatch_get_main_queue(), ^{
      [self.chromeActivityOverlayCoordinator stop];
      if (completionBlock)
        completionBlock();
    });
  };

  [self.dispatcher
      removeBrowsingDataForBrowserState:browserState
                             timePeriod:timePeriod
                             removeMask:removeMask
                        completionBlock:removeBrowsingDidFinishCompletionBlock];
}

- (void)showBrowsingHistoryRemovedDialog {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_DESCRIPTION);

  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self
                                                     title:title
                                                   message:message];

  __weak ClearBrowsingDataTableViewController* weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OPEN_HISTORY_BUTTON)
                action:^{
                  [weakSelf.localDispatcher openURL:GURL(kGoogleMyAccountURL)];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK_BUTTON)
                action:nil
                 style:UIAlertActionStyleCancel];

  [self.alertCoordinator start];
}

#pragma mark - Private Helpers

- (void)showClearBrowsingDataAlertController:(id)sender {
  BrowsingDataRemoveMask dataTypeMaskToRemove =
      BrowsingDataRemoveMask::REMOVE_NOTHING;
  NSArray* dataTypeItems = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierDataTypes];
  for (TableViewClearBrowsingDataItem* dataTypeItem in dataTypeItems) {
    DCHECK([dataTypeItem isKindOfClass:[TableViewClearBrowsingDataItem class]]);
    if (dataTypeItem.checked) {
      dataTypeMaskToRemove = dataTypeMaskToRemove | dataTypeItem.dataTypeMask;
    }
  }
  // Get button's position in coordinate system of table view.
  DCHECK_EQ(self.clearBrowsingDataButton, sender);
  CGRect clearBrowsingDataButtonRect = [self.clearBrowsingDataButton
      convertRect:self.clearBrowsingDataButton.bounds
           toView:self.tableView];
  self.actionSheetCoordinator = [self.dataManager
      actionSheetCoordinatorWithDataTypesToRemove:dataTypeMaskToRemove
                               baseViewController:self
                                       sourceRect:clearBrowsingDataButtonRect
                                       sourceView:self.tableView];
  if (self.actionSheetCoordinator) {
    [self.actionSheetCoordinator start];
  }
}

@end
