// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/model/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_sink.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_source.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_updater.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_audience.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_provider.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar_button_commands.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar_button_manager.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// Height for the header on top of the sign-in promo cell.
constexpr CGFloat kSignInPromoSectionHeaderHeight = 10;

// Types of ListItems used by the reading list UI.
enum ReadingListItemType {
  kItemTypeHeader = kItemTypeEnumZero,
  kItemTypeItem,
  kSwitchItemType,
  kSwitchItemFooterType,
  kItemTypeSignInPromo,
};
// Identifiers for sections in the reading list UI.
enum ReadingListSectionIdentifier {
  kSectionIdentifierSignInPromo = kSectionIdentifierEnumZero,
  kSectionIdentifierUnread,
  kSectionIdentifierRead,
};

// Returns the ReadingListSelectionState corresponding with the provided numbers
// of read and unread items.
ReadingListSelectionState GetSelectionStateForSelectedCounts(
    NSUInteger selected_unread_count,
    NSUInteger selected_read_count) {
  if (selected_read_count > 0 && selected_unread_count > 0)
    return ReadingListSelectionState::READ_AND_UNREAD_ITEMS;
  if (selected_read_count > 0)
    return ReadingListSelectionState::ONLY_READ_ITEMS;
  if (selected_unread_count > 0)
    return ReadingListSelectionState::ONLY_UNREAD_ITEMS;
  return ReadingListSelectionState::NONE;
}

}  // namespace

@interface ReadingListTableViewController () <ReadingListDataSink,
                                              ReadingListToolbarButtonCommands,
                                              TableViewURLDragDataSource>

// Redefine the model to return ReadingListListItems
@property(nonatomic, readonly) TableViewModel<TableViewItem*>* tableViewModel;

// The number of batch operation triggered by UI.
// One UI operation can trigger multiple batch operation, so this can be greater
// than 1.
@property(nonatomic, assign) int numberOfBatchOperationInProgress;
// Whether the data source has been modified while in editing mode.
@property(nonatomic, assign) BOOL dataSourceModifiedWhileEditing;
// The toolbar button manager.
@property(nonatomic, strong) ReadingListToolbarButtonManager* toolbarManager;
// The number of read and unread cells that are currently selected.
@property(nonatomic, assign) NSUInteger selectedUnreadItemCount;
@property(nonatomic, assign) NSUInteger selectedReadItemCount;
// The action sheet used to confirm whether items should be marked as read or
// unread.
@property(nonatomic, strong) ActionSheetCoordinator* markConfirmationSheet;
// Whether the table view is being edited after tapping on the edit button in
// the toolbar.
@property(nonatomic, assign, getter=isEditingWithToolbarButtons)
    BOOL editingWithToolbarButtons;
// Whether the table view is being edited by the swipe-to-delete button.
@property(nonatomic, readonly, getter=isEditingWithSwipe) BOOL editingWithSwipe;
// Handler for URL drag interactions.
@property(nonatomic, strong) TableViewURLDragDropHandler* dragDropHandler;
@end

@implementation ReadingListTableViewController
@dynamic tableViewModel;

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _toolbarManager = [[ReadingListToolbarButtonManager alloc] init];
    _toolbarManager.commandHandler = self;
  }
  return self;
}

#pragma mark - Accessors

- (void)setAudience:(id<ReadingListListViewControllerAudience>)audience {
  if (_audience == audience)
    return;
  _audience = audience;
  BOOL hasItems = self.dataSource.ready && self.dataSource.hasElements;
  [_audience readingListHasItems:hasItems];
}

- (void)setDataSource:(id<ReadingListDataSource>)dataSource {
  DCHECK(self.browser);
  if (_dataSource == dataSource)
    return;
  _dataSource.dataSink = nil;
  _dataSource = dataSource;
  _dataSource.dataSink = self;
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  if (self.editing == editing)
    return;
  [super setEditing:editing animated:animated];
  self.selectedUnreadItemCount = 0;
  self.selectedReadItemCount = 0;
  if (!editing) {
    [self reloadDataIfNeededAndNotEditing];
    self.markConfirmationSheet = nil;
    self.editingWithToolbarButtons = NO;
    [self removeEmptySections];
  }
  [self updateToolbarItems];

  // Force update a11y actions based on edit mode.
  for (int section = 0; section < self.tableViewModel.numberOfSections;
       section++) {
    if (![self.tableViewModel numberOfItemsInSection:section]) {
      continue;
    }
    NSInteger sectionIdentifier =
        [self.tableViewModel sectionIdentifierForSectionIndex:section];
    [self reconfigureCellsForItems:
              [self.tableViewModel
                  itemsInSectionWithIdentifier:sectionIdentifier]];
  }
}

- (void)setSelectedUnreadItemCount:(NSUInteger)selectedUnreadItemCount {
  if (_selectedUnreadItemCount == selectedUnreadItemCount)
    return;
  BOOL hadSelectedUnreadItems = _selectedUnreadItemCount > 0;
  _selectedUnreadItemCount = selectedUnreadItemCount;
  if ((_selectedUnreadItemCount > 0) != hadSelectedUnreadItems)
    [self updateToolbarItems];
}

- (void)setSelectedReadItemCount:(NSUInteger)selectedReadItemCount {
  if (_selectedReadItemCount == selectedReadItemCount)
    return;
  BOOL hadSelectedReadItems = _selectedReadItemCount > 0;
  _selectedReadItemCount = selectedReadItemCount;
  if ((_selectedReadItemCount > 0) != hadSelectedReadItems)
    [self updateToolbarItems];
}

- (void)setMarkConfirmationSheet:
    (ActionSheetCoordinator*)markConfirmationSheet {
  if (_markConfirmationSheet == markConfirmationSheet)
    return;
  [_markConfirmationSheet stop];
  _markConfirmationSheet = markConfirmationSheet;
}

- (BOOL)isEditingWithSwipe {
  return self.editing && !self.editingWithToolbarButtons;
}

#pragma mark - Public

- (void)reloadData {
  [self.tableView.contextMenuInteraction dismissMenu];
  [self loadModel];
  if (self.viewLoaded)
    [self.tableView reloadData];
}

- (void)willBeDismissed {
  [self.dataSource dataSinkWillBeDismissed];
  [self dismissMarkConfirmationSheet];
}

+ (NSString*)accessibilityIdentifier {
  return kReadingListViewID;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_READING_LIST);

  self.tableView.accessibilityIdentifier =
      [[self class] accessibilityIdentifier];
  self.tableView.estimatedRowHeight = 56;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.allowsMultipleSelection = YES;
  self.dragDropHandler = [[TableViewURLDragDropHandler alloc] init];
  self.dragDropHandler.origin = WindowActivityReadingListOrigin;
  self.dragDropHandler.dragDataSource = self;
  self.tableView.dragDelegate = self.dragDropHandler;
  self.tableView.dragInteractionEnabled = true;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleSingleLine;

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitPreferredContentSizeCategory.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(verifyTableIsEmpty)];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // In case the sign-in promo visibility is changed before the first layout,
  // we need to refresh the empty view margin after the layout is done, to apply
  // the correct top margin value according to the promo view's height.
  [self updateEmptyViewTopMargin];
  [IntentDonationHelper donateIntent:IntentType::kOpenReadingList];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  if (self.editingWithSwipe)
    [self exitEditingModeAnimated:YES];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    [self verifyTableIsEmpty];
  }
}
#endif

#pragma mark - UITableViewDataSource

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(editingStyle, UITableViewCellEditingStyleDelete);
  base::RecordAction(base::UserMetricsAction("MobileReadingListDeleteEntry"));

  [self deleteItemsAtIndexPaths:@[ indexPath ]
                     endEditing:NO
            removeEmptySections:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.editing) {
    // Update the selected item counts and the toolbar buttons.
    ReadingListSectionIdentifier sectionID =
        static_cast<ReadingListSectionIdentifier>([self.tableViewModel
            sectionIdentifierForSectionIndex:indexPath.section]);
    switch (sectionID) {
      case kSectionIdentifierUnread:
        self.selectedUnreadItemCount++;
        break;
      case kSectionIdentifierRead:
        self.selectedReadItemCount++;
        break;
      case kSectionIdentifierSignInPromo:
        NOTREACHED();
    }
  } else {
    // Open the URL.
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    // TODO(crbug.com/40263259): the runtime check will be replaced using new
    // methods implementations in TableViewItem and ReadingListTableViewItem.
    if ([item conformsToProtocol:@protocol(ReadingListListItem)]) {
      [self.delegate
          readingListListViewController:self
                               openItem:(id<ReadingListListItem>)item];
    }
  }
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.editing) {
    // Update the selected item counts and the toolbar buttons.
    ReadingListSectionIdentifier sectionID =
        static_cast<ReadingListSectionIdentifier>([self.tableViewModel
            sectionIdentifierForSectionIndex:indexPath.section]);
    switch (sectionID) {
      case kSectionIdentifierUnread:
        self.selectedUnreadItemCount--;
        break;
      case kSectionIdentifierRead:
        self.selectedReadItemCount--;
        break;
      case kSectionIdentifierSignInPromo:
        NOTREACHED();
    }
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  return [self.tableViewModel itemAtIndexPath:indexPath].type == kItemTypeItem;
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  if (self.isEditing) {
    // Don't show the context menu when currently in editing mode.
    return nil;
  }
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  // TODO(crbug.com/40263259): the runtime check will be replaced using new
  // methods implementations in TableViewItem and ReadingListTableViewItem.
  if ([item conformsToProtocol:@protocol(ReadingListListItem)]) {
    return [self.menuProvider
        contextMenuConfigurationForItem:(id<ReadingListListItem>)item
                               withView:[self.tableView
                                            cellForRowAtIndexPath:indexPath]];
  } else {
    return nil;
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
      kSectionIdentifierSignInPromo) {
    return kSignInPromoSectionHeaderHeight;
  }
  return UITableViewAutomaticDimension;
}

#pragma mark - TableViewURLDragDataSource

- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath {
  if (self.tableView.editing)
    return nil;
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  // TODO(crbug.com/40263259): the runtime check will be replaced using new
  // methods implementations in TableViewItem and ReadingListTableViewItem.
  if ([item conformsToProtocol:@protocol(ReadingListListItem)]) {
    id<ReadingListListItem> readingListItem = (id<ReadingListListItem>)item;
    return [[URLInfo alloc] initWithURL:readingListItem.entryURL
                                  title:readingListItem.title];
  } else {
    return nil;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction("IOSReadingListCloseWithSwipe"));
  // Call the delegate dismissReadingListListViewController to clean up state
  // and stop the Coordinator.
  [self.delegate dismissReadingListListViewController:self];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  self.dataSourceModifiedWhileEditing = NO;
  if (self.dataSource.hasElements) {
    [self tableIsNotEmpty];
  } else {
    [self tableIsEmpty];
  }
  [self.delegate didLoadContent];
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self.delegate dismissReadingListListViewController:self];
}

#pragma mark - ReadingListDataSink

- (void)dataSourceReady:(id<ReadingListDataSource>)dataSource {
  [self reloadData];
}

- (void)dataSourceChanged {
  // If the model is updated when the UI is already making a change, set a flag
  // to reload the data at the end of the editing.
  if (self.numberOfBatchOperationInProgress || self.isEditing) {
    self.dataSourceModifiedWhileEditing = YES;
  } else {
    [self reloadData];
  }
}

- (NSArray<id<ReadingListListItem>>*)readItems {
  return [self itemsForSection:kSectionIdentifierRead];
}

- (NSArray<id<ReadingListListItem>>*)unreadItems {
  return [self itemsForSection:kSectionIdentifierUnread];
}

- (void)itemHasChangedAfterDelay:(id<ReadingListListItem>)item {
  TableViewItem<ReadingListListItem>* tableItem =
      [self tableItemForReadingListItem:item];
  if ([self.tableViewModel hasItem:tableItem])
    [self reconfigureCellsForItems:@[ tableItem ]];
}

- (void)itemsHaveChanged:(NSArray<ListItem*>*)items {
  [self reconfigureCellsForItems:items];
}

#pragma mark - ReadingListDataSink Helpers

// Returns the items for the `sectionID`.
- (NSArray<id<ReadingListListItem>>*)itemsForSection:
    (ReadingListSectionIdentifier)sectionID {
  TableViewModel* model = self.tableViewModel;
  return [model hasSectionForSectionIdentifier:sectionID]
             ? [model itemsInSectionWithIdentifier:sectionID]
             : nil;
}

#pragma mark - ReadingListListItemAccessibilityDelegate

- (BOOL)isItemRead:(id<ReadingListListItem>)item {
  return [self.dataSource isItemRead:item];
}

- (void)openItemInNewTab:(id<ReadingListListItem>)item {
  [self.delegate readingListListViewController:self
                              openItemInNewTab:item
                                     incognito:NO];
}

- (void)openItemInNewIncognitoTab:(id<ReadingListListItem>)item {
  [self.delegate readingListListViewController:self
                              openItemInNewTab:item
                                     incognito:YES];
}

- (void)openItemOffline:(id<ReadingListListItem>)item {
  [self.delegate readingListListViewController:self
                       openItemOfflineInNewTab:item];
}

- (void)markItemRead:(id<ReadingListListItem>)item {
  TableViewModel* model = self.tableViewModel;
  if (![model hasSectionForSectionIdentifier:kSectionIdentifierUnread]) {
    // Prevent trying to access this section if it has been concurrently
    // deleted (via another window or Sync).
    return;
  }

  TableViewItem* tableViewItem =
      base::apple::ObjCCastStrict<TableViewItem>(item);
  if ([model hasItem:tableViewItem
          inSectionWithIdentifier:kSectionIdentifierUnread]) {
    [self markItemsAtIndexPaths:@[ [model indexPathForItem:tableViewItem] ]
                 withReadStatus:YES];
  }
}

- (void)markItemUnread:(id<ReadingListListItem>)item {
  TableViewModel* model = self.tableViewModel;
  if (![model hasSectionForSectionIdentifier:kSectionIdentifierRead]) {
    // Prevent trying to access this section if it has been concurrently
    // deleted (via another window or Sync).
    return;
  }

  TableViewItem* tableViewItem =
      base::apple::ObjCCastStrict<TableViewItem>(item);
  if ([model hasItem:tableViewItem
          inSectionWithIdentifier:kSectionIdentifierRead]) {
    [self markItemsAtIndexPaths:@[ [model indexPathForItem:tableViewItem] ]
                 withReadStatus:NO];
  }
}

- (void)deleteItem:(id<ReadingListListItem>)item {
  TableViewItem<ReadingListListItem>* tableViewItem =
      base::apple::ObjCCastStrict<TableViewItem<ReadingListListItem>>(item);
  if ([self.tableViewModel hasItem:tableViewItem]) {
    NSIndexPath* indexPath =
        [self.tableViewModel indexPathForItem:tableViewItem];
    [self deleteItemsAtIndexPaths:@[ indexPath ]];
  }
}

#pragma mark - ReadingListToolbarButtonCommands

- (void)enterReadingListEditMode {
  if (self.editing && !self.editingWithToolbarButtons) {
    // Reset swipe editing to trigger button editing
    [self setEditing:NO animated:NO];
  }
  if (self.editing) {
    return;
  }
  self.editingWithToolbarButtons = YES;
  [self setEditing:YES animated:YES];
}

- (void)exitReadingListEditMode {
  if (!self.editing)
    return;
  [self exitEditingModeAnimated:YES];
}

- (void)deleteAllReadReadingListItems {
  base::RecordAction(base::UserMetricsAction("MobileReadingListDeleteRead"));
  if (![self hasItemInSection:kSectionIdentifierRead]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  // Delete the items in the data source and exit editing mode.
  ReadingListListItemUpdater updater = ^(id<ReadingListListItem> item) {
    [self.dataSource removeEntryFromItem:item];
  };
  [self updateItemsInSection:kSectionIdentifierRead withItemUpdater:updater];

  // Update the model and table view for the deleted items.
  UITableView* tableView = self.tableView;
  TableViewModel* model = self.tableViewModel;
  void (^updates)(void) = ^{
    NSInteger sectionIndex =
        [model sectionForSectionIdentifier:kSectionIdentifierRead];
    [tableView deleteSections:[NSIndexSet indexSetWithIndex:sectionIndex]
             withRowAnimation:UITableViewRowAnimationMiddle];
    [model removeSectionWithIdentifier:kSectionIdentifierRead];
  };
  void (^completion)(BOOL) = ^(BOOL) {
    [self batchEditDidFinish];
  };
  [self performBatchTableViewUpdates:updates completion:completion];
  [self exitEditingModeAnimated:YES];
}

- (void)deleteSelectedReadingListItems {
  base::RecordAction(
      base::UserMetricsAction("MobileReadingListDeleteSelected"));
  [self deleteItemsAtIndexPaths:self.tableView.indexPathsForSelectedRows];
  [self exitEditingModeAnimated:YES];
}

- (void)markSelectedReadingListItemsRead {
  [self markItemsAtIndexPaths:self.tableView.indexPathsForSelectedRows
               withReadStatus:YES];
}

- (void)markSelectedReadingListItemsUnread {
  [self markItemsAtIndexPaths:self.tableView.indexPathsForSelectedRows
               withReadStatus:NO];
}

- (void)markSelectedReadingListItemsAfterConfirmation {
  [self initializeMarkConfirmationSheet];
  __weak ReadingListTableViewController* weakSelf = self;
  NSArray<NSIndexPath*>* selectedIndexPaths =
      self.tableView.indexPathsForSelectedRows;
  NSString* markAsReadTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  [self.markConfirmationSheet
      addItemWithTitle:markAsReadTitle
                action:^{
                  [weakSelf markItemsAtIndexPaths:selectedIndexPaths
                                   withReadStatus:YES];
                  [weakSelf dismissMarkConfirmationSheet];
                }
                 style:UIAlertActionStyleDefault];
  NSString* markAsUnreadTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  [self.markConfirmationSheet
      addItemWithTitle:markAsUnreadTitle
                action:^{
                  [weakSelf markItemsAtIndexPaths:selectedIndexPaths
                                   withReadStatus:NO];
                  [weakSelf dismissMarkConfirmationSheet];
                }
                 style:UIAlertActionStyleDefault];
  [self.markConfirmationSheet start];
}

- (void)markAllReadingListItemsAfterConfirmation {
  [self initializeMarkConfirmationSheet];
  __weak ReadingListTableViewController* weakSelf = self;
  NSString* markAsReadTitle = l10n_util::GetNSStringWithFixup(
      IDS_IOS_READING_LIST_MARK_ALL_READ_ACTION);
  [self.markConfirmationSheet
      addItemWithTitle:markAsReadTitle
                action:^{
                  [weakSelf markItemsInSection:kSectionIdentifierUnread
                                withReadStatus:YES];
                  [weakSelf dismissMarkConfirmationSheet];
                }
                 style:UIAlertActionStyleDefault];
  NSString* markAsUnreadTitle = l10n_util::GetNSStringWithFixup(
      IDS_IOS_READING_LIST_MARK_ALL_UNREAD_ACTION);
  [self.markConfirmationSheet
      addItemWithTitle:markAsUnreadTitle
                action:^{
                  [weakSelf markItemsInSection:kSectionIdentifierRead
                                withReadStatus:NO];
                  [weakSelf dismissMarkConfirmationSheet];
                }
                 style:UIAlertActionStyleDefault];
  [self.markConfirmationSheet start];
}

- (void)performBatchTableViewUpdates:(void (^)(void))updates
                          completion:(void (^)(BOOL finished))completion {
  self.numberOfBatchOperationInProgress += 1;
  void (^releaseDataSource)(BOOL) = ^(BOOL finished) {
    // Set numberOfBatchOperationInProgress before calling completion, as
    // completion may trigger another change.
    DCHECK_GT(self.numberOfBatchOperationInProgress, 0);
    self.numberOfBatchOperationInProgress -= 1;
    if (completion) {
      completion(finished);
    }
  };
  [super performBatchTableViewUpdates:updates completion:releaseDataSource];
}

#pragma mark - ReadingListToolbarButtonCommands Helpers

// Creates a confirmation action sheet for the "Mark" toolbar button item.
- (void)initializeMarkConfirmationSheet {
  self.markConfirmationSheet = [self.toolbarManager
      markButtonConfirmationWithBaseViewController:self
                                           browser:_browser];

  __weak ReadingListTableViewController* weakSelf = self;
  [self.markConfirmationSheet
      addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
                action:^{
                  [weakSelf dismissMarkConfirmationSheet];
                }
                 style:UIAlertActionStyleCancel];
}

#pragma mark - Sign-in Promo

- (void)promoStateChanged:(BOOL)promoEnabled
        promoConfigurator:(SigninPromoViewConfigurator*)promoConfigurator
            promoDelegate:(id<SigninPromoViewDelegate>)promoDelegate
                promoText:(NSString*)promoText {
  if (promoEnabled) {
    CHECK(![self.tableViewModel
        hasSectionForSectionIdentifier:kSectionIdentifierSignInPromo]);
    [self.tableViewModel
        insertSectionWithIdentifier:kSectionIdentifierSignInPromo
                            atIndex:0];
    TableViewSigninPromoItem* signInPromoItem =
        [[TableViewSigninPromoItem alloc] initWithType:kItemTypeSignInPromo];
    signInPromoItem.configurator = promoConfigurator;
    signInPromoItem.text = promoText;
    signInPromoItem.delegate = promoDelegate;
    [self.tableViewModel addItem:signInPromoItem
         toSectionWithIdentifier:kSectionIdentifierSignInPromo];
  } else {
    CHECK([self.tableViewModel
        hasSectionForSectionIdentifier:kSectionIdentifierSignInPromo]);
    [self.tableViewModel
        removeSectionWithIdentifier:kSectionIdentifierSignInPromo];
  }
  [self.tableView reloadData];
  [self updateEmptyViewTopMargin];
}

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)promoConfigurator
                             identityChanged:(BOOL)identityChanged {
  if (![self.tableViewModel
          hasSectionForSectionIdentifier:kSectionIdentifierSignInPromo]) {
    return;
  }

  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:kItemTypeSignInPromo
                              sectionIdentifier:kSectionIdentifierSignInPromo];
  TableViewSigninPromoItem* signInPromoItem =
      base::apple::ObjCCast<TableViewSigninPromoItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  if (!signInPromoItem) {
    return;
  }

  signInPromoItem.configurator = promoConfigurator;
  [self reloadCellsForItems:@[ signInPromoItem ]
           withRowAnimation:UITableViewRowAnimationNone];

  // The sign-in promo view height may have been changed after the configurator
  // change, we need to update the empty view top margin according to it.
  [self updateEmptyViewTopMargin];
}

#pragma mark - Item Loading Helpers

// Uses self.dataSource to load the TableViewItems into self.tableViewModel.
- (void)loadItems {
  NSMutableArray<id<ReadingListListItem>>* readArray = [NSMutableArray array];
  NSMutableArray<id<ReadingListListItem>>* unreadArray = [NSMutableArray array];
  [self.dataSource fillReadItems:readArray unreadItems:unreadArray];
  [self loadItemsFromArray:unreadArray toSection:kSectionIdentifierUnread];
  [self loadItemsFromArray:readArray toSection:kSectionIdentifierRead];

  [self updateToolbarItems];
}

// Adds `items` to self.tableViewModel for the section designated by
// `sectionID`.
- (void)loadItemsFromArray:(NSArray<id<ReadingListListItem>>*)items
                 toSection:(ReadingListSectionIdentifier)sectionID {
  if (!items.count)
    return;

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:sectionID];
  [model setHeader:[self headerForSectionIndex:sectionID]
      forSectionWithIdentifier:sectionID];
  __weak __typeof(self) weakSelf = self;
  for (TableViewItem<ReadingListListItem>* item in items) {
    item.type = kItemTypeItem;
    [model addItem:item toSectionWithIdentifier:sectionID];

    // This function is currently reloading the model.
    // It has been observed that the item just added is not fully available,
    // the model containing the item but the item count of the section not
    // being updated correctly.
    // Updating the favicon can lead to synchronous update of the item if the
    // icon is already available. To avoid causing a crash, update the trigger
    // the favicon asynchronously.
    // TODO(crbug.com/40240200): check the fix actually prevents crashing.
    __weak __typeof(item) weakItem = item;
    dispatch_async(dispatch_get_main_queue(), ^{
      if (weakSelf && weakItem) {
        [weakSelf.dataSource fetchFaviconForItem:weakItem];
      }
    });
  }
}

// Returns a TableViewTextItem that displays the title for the section
// designated by `sectionID`.
- (TableViewHeaderFooterItem*)headerForSectionIndex:
    (ReadingListSectionIdentifier)sectionID {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:kItemTypeHeader];

  switch (sectionID) {
    case kSectionIdentifierRead:
      header.text = l10n_util::GetNSString(IDS_IOS_READING_LIST_READ_HEADER);
      break;
    case kSectionIdentifierUnread:
      header.text = l10n_util::GetNSString(IDS_IOS_READING_LIST_UNREAD_HEADER);
      break;
    case kSectionIdentifierSignInPromo:
      header = nil;
      break;
  }
  return header;
}

#pragma mark - Toolbar Helpers

// Updates buttons displayed in the bottom toolbar.
- (void)updateToolbarItems {
  self.toolbarManager.editing = self.editingWithToolbarButtons;
  self.toolbarManager.hasReadItems =
      self.dataSource.hasElements && self.dataSource.hasReadElements;
  self.toolbarManager.selectionState = GetSelectionStateForSelectedCounts(
      self.selectedUnreadItemCount, self.selectedReadItemCount);
  if (self.toolbarManager.buttonItemsUpdated)
    [self setToolbarItems:[self.toolbarManager buttonItems] animated:YES];
  [self.toolbarManager updateMarkButtonTitle];
}

#pragma mark - Item Editing Helpers

// Returns `item` cast as a TableViewItem.
- (TableViewItem<ReadingListListItem>*)tableItemForReadingListItem:
    (id<ReadingListListItem>)item {
  return base::apple::ObjCCastStrict<TableViewItem<ReadingListListItem>>(item);
}

// Applies `updater` to the items in `section`. The updates are done in reverse
// order of the cells in the section to keep the order. Monitoring of the
// data source updates are suspended during this time.
- (void)updateItemsInSection:(ReadingListSectionIdentifier)section
             withItemUpdater:(ReadingListListItemUpdater)updater {
  DCHECK(updater);
  [self.dataSource beginBatchUpdates];
  NSArray* items = [self.tableViewModel itemsInSectionWithIdentifier:section];
  // Read the objects in reverse order to keep the order (last modified first).
  for (TableViewItem* item in [items reverseObjectEnumerator]) {
    // TODO(crbug.com/40263259): the runtime check will be replaced using new
    // methods implementations in TableViewItem and ReadingListTableViewItem.
    if ([item conformsToProtocol:@protocol(ReadingListListItem)]) {
      updater((id<ReadingListListItem>)item);
    }
  }
  [self.dataSource endBatchUpdates];
}

// Applies `updater` to the items in `indexPaths`. The updates are done in
// reverse order `indexPaths` to keep the order. The monitoring of the data
// source updates are suspended during this time.
- (void)updateItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
                withItemUpdater:(ReadingListListItemUpdater)updater {
  DCHECK(updater);
  [self.dataSource beginBatchUpdates];
  // Read the objects in reverse order to keep the order (last modified first).
  for (NSIndexPath* indexPath in [indexPaths reverseObjectEnumerator]) {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    // TODO(crbug.com/40263259): the runtime check will be replaced by new
    // methods implementations in TableViewItem and ReadingListTableViewItem.
    if ([item conformsToProtocol:@protocol(ReadingListListItem)]) {
      updater((id<ReadingListListItem>)item);
    }
  }
  [self.dataSource endBatchUpdates];
}

// Moves all the items from `fromSection` to `toSection` and removes the empty
// section from the collection.
- (void)moveItemsFromSection:(ReadingListSectionIdentifier)fromSection
                   toSection:(ReadingListSectionIdentifier)toSection {
  if (![self.tableViewModel hasSectionForSectionIdentifier:fromSection]) {
    return;
  }
  NSInteger sourceSection =
      [self.tableViewModel sectionForSectionIdentifier:fromSection];
  NSInteger itemCount =
      [self.tableViewModel numberOfItemsInSection:sourceSection];

  NSMutableArray* sortedIndexPaths = [NSMutableArray array];
  for (NSInteger row = 0; row < itemCount; ++row) {
    NSIndexPath* itemPath =
        [NSIndexPath indexPathForRow:row inSection:sourceSection];
    [sortedIndexPaths addObject:itemPath];
  }

  [self moveItemsAtIndexPaths:sortedIndexPaths toSection:toSection];
}

// Moves the items at `sortedIndexPaths` to `toSection`, removing any empty
// sections.
- (void)moveItemsAtIndexPaths:(NSArray*)sortedIndexPaths
                    toSection:(ReadingListSectionIdentifier)toSection {
  // Reconfigure cells, allowing the custom actions to be updated.
  for (NSIndexPath* indexPath in sortedIndexPaths) {
    if (![self.tableView cellForRowAtIndexPath:indexPath])
      continue;

    [[self.tableViewModel itemAtIndexPath:indexPath]
        configureCell:[self.tableView cellForRowAtIndexPath:indexPath]
           withStyler:self.styler];
  }

  NSInteger sectionCreatedIndex = [self initializeTableViewSection:toSection];
  void (^updates)(void) = ^{
    NSInteger sectionIndex =
        [self.tableViewModel sectionForSectionIdentifier:toSection];

    NSInteger newItemIndex = 0;
    for (NSIndexPath* indexPath in sortedIndexPaths) {
      // The `sortedIndexPaths` is a copy of the index paths before the
      // destination section has been added if necessary. The section part of
      // the index potentially needs to be updated.
      NSInteger updatedSection = indexPath.section;
      if (updatedSection >= sectionCreatedIndex)
        updatedSection++;
      if (updatedSection == sectionIndex) {
        // The item is already in the targeted section, there is no need to move
        // it.
        continue;
      }

      NSIndexPath* updatedIndexPath =
          [NSIndexPath indexPathForItem:indexPath.row inSection:updatedSection];
      NSIndexPath* indexPathForModel =
          [NSIndexPath indexPathForItem:indexPath.item - newItemIndex
                              inSection:updatedSection];

      // Index of the item in the new section. The newItemIndex is the index of
      // this item in the targeted section.
      NSIndexPath* newIndexPath =
          [NSIndexPath indexPathForItem:newItemIndex++ inSection:sectionIndex];

      [self moveItemWithModelIndex:indexPathForModel
                    tableViewIndex:updatedIndexPath
                           toIndex:newIndexPath];
    }
  };
  void (^completion)(BOOL) = ^(BOOL) {
    [self batchEditDidFinish];
  };
  [self performBatchTableViewUpdates:updates completion:completion];
}

// Moves the ListItem within self.tableViewModel at `modelIndex` and the
// UITableViewCell at `tableViewIndex` to `toIndexPath`.
- (void)moveItemWithModelIndex:(NSIndexPath*)modelIndex
                tableViewIndex:(NSIndexPath*)tableViewIndex
                       toIndex:(NSIndexPath*)toIndexPath {
  TableViewModel* model = self.tableViewModel;
  TableViewItem* item = [model itemAtIndexPath:modelIndex];

  // Move the item in `model`.
  [self deleteItemAtIndexPathFromModel:modelIndex];
  NSInteger toSectionID =
      [model sectionIdentifierForSectionIndex:toIndexPath.section];
  [model insertItem:item
      inSectionWithIdentifier:toSectionID
                      atIndex:toIndexPath.row];

  // Move the cells in the table view.
  [self.tableView moveRowAtIndexPath:tableViewIndex toIndexPath:toIndexPath];
}

// Makes sure the table view section with `sectionID` exists with the correct
// header. Returns the index of the new section in the table view, or
// NSIntegerMax if no section has been created.
- (NSInteger)initializeTableViewSection:
    (ReadingListSectionIdentifier)sectionID {
  TableViewModel* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionID])
    return NSIntegerMax;

  NSInteger sectionIndex = [self newSectionIndexForId:sectionID];
  void (^updates)(void) = ^{
    [model insertSectionWithIdentifier:sectionID atIndex:sectionIndex];
    [model setHeader:[self headerForSectionIndex:sectionID]
        forSectionWithIdentifier:sectionID];
    [self.tableView insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                  withRowAnimation:UITableViewRowAnimationMiddle];
  };
  [self performBatchTableViewUpdates:updates completion:nil];

  return sectionIndex;
}

// Whether the model has items in `sectionID`.
- (BOOL)hasItemInSection:(ReadingListSectionIdentifier)sectionID {
  return [self itemsForSection:sectionID].count > 0;
}

// Deletes the items at `indexPaths`, exiting editing and removing empty
// sections upon completion.
- (void)deleteItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  [self deleteItemsAtIndexPaths:indexPaths
                     endEditing:YES
            removeEmptySections:YES];
}

// Deletes the items at `indexPaths`.  Exits editing mode if `endEditing` is
// YES.  Removes empty sections upon completion if `removeEmptySections` is YES.
- (void)deleteItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
                     endEditing:(BOOL)endEditing
            removeEmptySections:(BOOL)removeEmptySections {
  // Delete the items in the data source and exit editing mode.
  ReadingListListItemUpdater updater = ^(id<ReadingListListItem> item) {
    [self.dataSource removeEntryFromItem:item];
  };
  [self updateItemsAtIndexPaths:indexPaths withItemUpdater:updater];
  // Update the model and table view for the deleted items.
  UITableView* tableView = self.tableView;
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  void (^updates)(void) = ^{
    // Enumerate in reverse order to delete the items from the model.
    for (NSIndexPath* indexPath in [sortedIndexPaths reverseObjectEnumerator]) {
      [self deleteItemAtIndexPathFromModel:indexPath];
    }
    [tableView deleteRowsAtIndexPaths:indexPaths
                     withRowAnimation:UITableViewRowAnimationAutomatic];
  };

  void (^completion)(BOOL) = nil;
  if (removeEmptySections) {
    completion = ^(BOOL) {
      [self batchEditDidFinish];
    };
  }
  [self performBatchTableViewUpdates:updates completion:completion];
  if (endEditing) {
    [self exitEditingModeAnimated:YES];
  }
}

// Deletes the ListItem corresponding to `indexPath` in the model.
- (void)deleteItemAtIndexPathFromModel:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger sectionID =
      [model sectionIdentifierForSectionIndex:indexPath.section];
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  NSUInteger index = [model indexInItemTypeForIndexPath:indexPath];
  [model removeItemWithType:itemType
      fromSectionWithIdentifier:sectionID
                        atIndex:index];
}

// Marks all the items at `indexPaths` as read or unread depending on `read`.
- (void)markItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
               withReadStatus:(BOOL)read {
  // Record metric.
  base::RecordAction(base::UserMetricsAction(
      read ? "MobileReadingListMarkRead" : "MobileReadingListMarkUnread"));

  // Mark the items as `read` and exit editing.
  ReadingListListItemUpdater updater = ^(id<ReadingListListItem> item) {
    [self.dataSource setReadStatus:read forItem:item];
  };
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  [self updateItemsAtIndexPaths:sortedIndexPaths withItemUpdater:updater];

  // Move the items to the appropriate section.
  ReadingListSectionIdentifier toSection =
      read ? kSectionIdentifierRead : kSectionIdentifierUnread;
  [self moveItemsAtIndexPaths:sortedIndexPaths toSection:toSection];
  [self exitEditingModeAnimated:YES];
}

// Marks items from `section` with as read or unread dending on `read`.
- (void)markItemsInSection:(ReadingListSectionIdentifier)section
            withReadStatus:(BOOL)read {
  if (![self.tableViewModel hasSectionForSectionIdentifier:section]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  // Mark the items as `read` and exit editing.
  ReadingListListItemUpdater updater = ^(id<ReadingListListItem> item) {
    [self.dataSource setReadStatus:read forItem:item];
  };
  [self updateItemsInSection:section withItemUpdater:updater];

  // Move the items to the appropriate section.
  ReadingListSectionIdentifier toSection =
      read ? kSectionIdentifierRead : kSectionIdentifierUnread;
  [self moveItemsFromSection:section toSection:toSection];
  [self exitEditingModeAnimated:YES];
}

// Cleanup function called in the completion block of editing operations.
- (void)batchEditDidFinish {
  // Reload the items if the datasource was modified during the edit.
  [self reloadDataIfNeededAndNotEditing];

  // Remove any newly emptied sections.
  [self removeEmptySections];
}

// Removes the empty sections from the table and the model.  Returns the number
// of removed sections.
- (NSUInteger)removeEmptySections {
  UITableView* tableView = self.tableView;
  TableViewModel* model = self.tableViewModel;
  __block NSUInteger removedSectionCount = 0;
  void (^updates)(void) = ^{
    ReadingListSectionIdentifier sections[] = {kSectionIdentifierRead,
                                               kSectionIdentifierUnread};
    for (size_t i = 0; i < std::size(sections); ++i) {
      ReadingListSectionIdentifier section = sections[i];

      if ([model hasSectionForSectionIdentifier:section] &&
          ![self hasItemInSection:section]) {
        // If `section` has no items, remove it from the model and the table
        // view.
        NSInteger sectionIndex = [model sectionForSectionIdentifier:section];
        [tableView deleteSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                 withRowAnimation:UITableViewRowAnimationFade];
        [model removeSectionWithIdentifier:section];
        ++removedSectionCount;
      }
    }
  };

  [self performBatchTableViewUpdates:updates completion:nil];
  if (!self.dataSource.hasElements)
    [self tableIsEmpty];
  else
    [self updateToolbarItems];
  return removedSectionCount;
}

// Resets self.editing to NO, optionally with animation.
- (void)exitEditingModeAnimated:(BOOL)animated {
  self.markConfirmationSheet = nil;
  [self setEditing:NO animated:animated];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  base::RecordAction(
      base::UserMetricsAction("MobileReadingListAccessibilityClose"));
  [self.delegate dismissReadingListListViewController:self];
  return YES;
}

#pragma mark - Private

// Called when the table is not empty.
- (void)tableIsNotEmpty {
  [self loadItems];
  [self.audience readingListHasItems:YES];
  self.tableView.alwaysBounceVertical = YES;
  [self removeEmptyTableView];
}

// Called when the table is empty.
- (void)tableIsEmpty {
  // It is necessary to reloadData now, before modifying the view which will
  // force a layout.
  // If the window is not displayed (e.g. in an inactive scene) the number of
  // elements may be outdated and the layout triggered by this function will
  // generate access non-existing items.
  [self.tableView reloadData];
  UIImage* emptyImage = [UIImage imageNamed:@"reading_list_empty"];
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_NO_ENTRIES_TITLE);
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_NO_ENTRIES_MESSAGE);
  [self addEmptyTableViewWithImage:emptyImage title:title subtitle:subtitle];
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.tableView.alwaysBounceVertical = NO;
  [self.audience readingListHasItems:NO];
  [self updateEmptyViewTopMargin];
}

// Reloads the data if source change during the edit mode and if it is now safe
// to do so (local edits are done).
- (void)reloadDataIfNeededAndNotEditing {
  if (self.dataSourceModifiedWhileEditing &&
      self.numberOfBatchOperationInProgress == 0 && !self.editing) {
    [self reloadData];
  }
}

// The empty view has different top margin according to the sign-in promo view
// presence. This method needs to be called after the promo view changes.
- (void)updateEmptyViewTopMargin {
  BOOL promoViewVisible =
      [self.tableViewModel hasItemForItemType:kItemTypeSignInPromo
                            sectionIdentifier:kSectionIdentifierSignInPromo];
  if (promoViewVisible && !self.dataSource.hasElements) {
    NSIndexPath* promoIndexPath = [self.tableViewModel
        indexPathForItemType:kItemTypeSignInPromo
           sectionIdentifier:kSectionIdentifierSignInPromo];
    UITableViewCell* promoCell =
        [self.tableView cellForRowAtIndexPath:promoIndexPath];
    CGFloat promoHeight = promoCell.bounds.size.height;
    [self setEmptyViewTopOffset:promoHeight];
  } else {
    [self setEmptyViewTopOffset:0.0];
  }
}

// Computes the index of the section to be created, given the sections that
// already exist.
- (NSInteger)newSectionIndexForId:(ReadingListSectionIdentifier)newSectionID {
  ReadingListSectionIdentifier sections[] = {kSectionIdentifierSignInPromo,
                                             kSectionIdentifierUnread,
                                             kSectionIdentifierRead};
  NSInteger sectionIndex = 0;
  for (ReadingListSectionIdentifier section : sections) {
    if (newSectionID == section) {
      return sectionIndex;
    }
    if ([self hasItemInSection:section]) {
      sectionIndex++;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

- (void)dismissMarkConfirmationSheet {
  [_markConfirmationSheet stop];
  _markConfirmationSheet = nil;
}

// Invokes the `tableIsEmpty` function when the data source doesn't have any
// elements.
- (void)verifyTableIsEmpty {
  if (self.dataSource.hasElements) {
    return;
  }

  [self tableIsEmpty];
}

@end
