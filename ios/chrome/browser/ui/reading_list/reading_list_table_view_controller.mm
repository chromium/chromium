// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/stl_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/reading_list/empty_reading_list_message_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_sink.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_source.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_updater.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_audience.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar_button_commands.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar_button_manager.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The image to use in the placeholder view while the table is empty.
NSString* const kEmptyStateImage = @"reading_list_empty_state_new";
// Types of ListItems used by the reading list UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeItem,
};
// Identifiers for sections in the reading list.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierUnread = kSectionIdentifierEnumZero,
  SectionIdentifierRead,
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

@interface ReadingListTableViewController ()<ReadingListDataSink,
                                             ReadingListToolbarButtonCommands>

// Redefine the model to return ReadingListListItems
@property(nonatomic, readonly)
    TableViewModel<TableViewItem<ReadingListListItem>*>* tableViewModel;

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
// Whether to remove empty sections after editing is reset to NO.
@property(nonatomic, assign) BOOL needsSectionCleanupAfterEditing;

@end

@implementation ReadingListTableViewController
@synthesize delegate = _delegate;
@synthesize audience = _audience;
@synthesize dataSource = _dataSource;
@dynamic tableViewModel;
@synthesize dataSourceModifiedWhileEditing = _dataSourceModifiedWhileEditing;
@synthesize toolbarManager = _toolbarManager;
@synthesize selectedUnreadItemCount = _selectedUnreadItemCount;
@synthesize selectedReadItemCount = _selectedReadItemCount;
@synthesize markConfirmationSheet = _markConfirmationSheet;
@synthesize editingWithToolbarButtons = _editingWithToolbarButtons;
@synthesize needsSectionCleanupAfterEditing = _needsSectionCleanupAfterEditing;

- (instancetype)init {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
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
  [self updateToolbarItems];
  if (!editing) {
    self.editingWithToolbarButtons = NO;
    if (self.needsSectionCleanupAfterEditing) {
      self.needsSectionCleanupAfterEditing = NO;
    }
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
  [self loadModel];
  if (self.viewLoaded)
    [self.tableView reloadData];
}

- (void)willBeDismissed {
  [self.dataSource dataSinkWillBeDismissed];
  self.markConfirmationSheet = nil;
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
  self.tableView.estimatedSectionHeaderHeight = 56;
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.allowsMultipleSelection = YES;
  // Add a tableFooterView in order to disable separators at the bottom of the
  // tableView.
  // TODO(crbug.com/863606): Remove this workaround when iOS10 is no longer
  // supported, as it is not necessary in iOS 11.
  self.tableView.tableFooterView = [[UIView alloc] init];

  // Add gesture recognizer for the context menu.
  UILongPressGestureRecognizer* longPressRecognizer =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [self.tableView addGestureRecognizer:longPressRecognizer];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  if (self.editingWithSwipe)
    [self exitEditingModeAnimated:YES];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (!self.dataSource.hasElements &&
      self.traitCollection.preferredContentSizeCategory !=
          previousTraitCollection.preferredContentSizeCategory) {
    [self tableIsEmpty];
  }
}

#pragma mark - UITableViewDataSource

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(editingStyle, UITableViewCellEditingStyleDelete);
  base::RecordAction(base::UserMetricsAction("MobileReadingListDeleteEntry"));
  // The UIKit animation for the swipe-to-delete gesture throws an exception if
  // the section of the deleted item is removed before the animation is
  // finished.  To prevent this from happening, record that cleanup is needed
  // and remove the section when self.tableView.editing is reset to NO when the
  // animation finishes.
  self.needsSectionCleanupAfterEditing = YES;
  [self deleteItemsAtIndexPaths:@[ indexPath ]
                     endEditing:NO
            removeEmptySections:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.editing) {
    // Update the selected item counts and the toolbar buttons.
    NSInteger sectionID =
        [self.tableViewModel sectionIdentifierForSection:indexPath.section];
    if (sectionID == SectionIdentifierUnread)
      self.selectedUnreadItemCount++;
    if (sectionID == SectionIdentifierRead)
      self.selectedReadItemCount++;
  } else {
    // Open the URL.
    id<ReadingListListItem> item =
        [self.tableViewModel itemAtIndexPath:indexPath];
    [self.delegate readingListListViewController:self openItem:item];
  }
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.editing) {
    // Update the selected item counts and the toolbar buttons.
    NSInteger sectionID =
        [self.tableViewModel sectionIdentifierForSection:indexPath.section];
    if (sectionID == SectionIdentifierUnread)
      self.selectedUnreadItemCount--;
    if (sectionID == SectionIdentifierRead)
      self.selectedReadItemCount--;
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  return [self.tableViewModel itemAtIndexPath:indexPath].type == ItemTypeItem;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // Call the delegate dismissReadingListListViewController to clean up state
  // and stop the Coordinator.
  [self.delegate dismissReadingListListViewController:self];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  self.dataSourceModifiedWhileEditing = NO;

  if (self.dataSource.hasElements) {
    [self loadItems];
    [self.audience readingListHasItems:YES];
    self.tableView.alwaysBounceVertical = YES;
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleSingleLine;
    self.tableView.backgroundView = nil;
  } else {
    [self tableIsEmpty];
  }
}

#pragma mark - ReadingListDataSink

- (void)dataSourceReady:(id<ReadingListDataSource>)dataSource {
  [self reloadData];
}

- (void)dataSourceChanged {
  // If we are editing and monitoring the model updates, set a flag to reload
  // the data at the end of the editing.
  if (self.editing) {
    self.dataSourceModifiedWhileEditing = YES;
  } else {
    [self reloadData];
  }
}

- (NSArray<id<ReadingListListItem>>*)readItems {
  return [self itemsForSection:SectionIdentifierRead];
}

- (NSArray<id<ReadingListListItem>>*)unreadItems {
  return [self itemsForSection:SectionIdentifierUnread];
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

// Returns the items for the |sectionID|.
- (NSArray<id<ReadingListListItem>>*)itemsForSection:
    (SectionIdentifier)sectionID {
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
  TableViewItem* tableViewItem = base::mac::ObjCCastStrict<TableViewItem>(item);
  if ([model hasItem:tableViewItem
          inSectionWithIdentifier:SectionIdentifierUnread]) {
    [self markItemsAtIndexPaths:@[ [model indexPathForItem:tableViewItem] ]
                 withReadStatus:YES];
  }
}

- (void)markItemUnread:(id<ReadingListListItem>)item {
  TableViewModel* model = self.tableViewModel;
  TableViewItem* tableViewItem = base::mac::ObjCCastStrict<TableViewItem>(item);
  if ([model hasItem:tableViewItem
          inSectionWithIdentifier:SectionIdentifierRead]) {
    [self markItemsAtIndexPaths:@[ [model indexPathForItem:tableViewItem] ]
                 withReadStatus:NO];
  }
}

#pragma mark - ReadingListToolbarButtonCommands

- (void)enterReadingListEditMode {
  if (self.editing)
    return;
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
  if (![self hasItemInSection:SectionIdentifierRead]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  // Delete the items in the data source and exit editing mode.
  ReadingListListItemUpdater updater = ^(id<ReadingListListItem> item) {
    [self.dataSource removeEntryFromItem:item];
  };
  [self updateItemsInSection:SectionIdentifierRead withItemUpdater:updater];
  [self exitEditingModeAnimated:YES];

  // Update the model and table view for the deleted items.
  UITableView* tableView = self.tableView;
  TableViewModel* model = self.tableViewModel;
  void (^updates)(void) = ^{
    NSInteger sectionIndex =
        [model sectionForSectionIdentifier:SectionIdentifierRead];
    [tableView deleteSections:[NSIndexSet indexSetWithIndex:sectionIndex]
             withRowAnimation:UITableViewRowAnimationMiddle];
    [model removeSectionWithIdentifier:SectionIdentifierRead];
  };
  void (^completion)(BOOL) = ^(BOOL) {
    [self batchEditDidFinish];
  };
  [self performBatchTableViewUpdates:updates completion:completion];
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
                  weakSelf.markConfirmationSheet = nil;
                }
                 style:UIAlertActionStyleDefault];
  NSString* markAsUnreadTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  [self.markConfirmationSheet
      addItemWithTitle:markAsUnreadTitle
                action:^{
                  [weakSelf markItemsAtIndexPaths:selectedIndexPaths
                                   withReadStatus:NO];
                  weakSelf.markConfirmationSheet = nil;
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
                  [weakSelf markItemsInSection:SectionIdentifierUnread
                                withReadStatus:YES];
                  weakSelf.markConfirmationSheet = nil;
                }
                 style:UIAlertActionStyleDefault];
  NSString* markAsUnreadTitle = l10n_util::GetNSStringWithFixup(
      IDS_IOS_READING_LIST_MARK_ALL_UNREAD_ACTION);
  [self.markConfirmationSheet
      addItemWithTitle:markAsUnreadTitle
                action:^{
                  [weakSelf markItemsInSection:SectionIdentifierRead
                                withReadStatus:NO];
                  weakSelf.markConfirmationSheet = nil;
                }
                 style:UIAlertActionStyleDefault];
  [self.markConfirmationSheet start];
}

#pragma mark - ReadingListToolbarButtonCommands Helpers

// Creates a confirmation action sheet for the "Mark" toolbar button item.
- (void)initializeMarkConfirmationSheet {
  self.markConfirmationSheet =
      [self.toolbarManager markButtonConfirmationWithBaseViewController:self];

  [self.markConfirmationSheet
      addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
                action:nil
                 style:UIAlertActionStyleCancel];
}

#pragma mark - Item Loading Helpers

// Uses self.dataSource to load the TableViewItems into self.tableViewModel.
- (void)loadItems {
  NSMutableArray<id<ReadingListListItem>>* readArray = [NSMutableArray array];
  NSMutableArray<id<ReadingListListItem>>* unreadArray = [NSMutableArray array];
  [self.dataSource fillReadItems:readArray unreadItems:unreadArray];
  [self loadItemsFromArray:unreadArray toSection:SectionIdentifierUnread];
  [self loadItemsFromArray:readArray toSection:SectionIdentifierRead];

  [self updateToolbarItems];
}

// Adds |items| to self.tableViewModel for the section designated by
// |sectionID|.
- (void)loadItemsFromArray:(NSArray<id<ReadingListListItem>>*)items
                 toSection:(SectionIdentifier)sectionID {
  if (!items.count)
    return;

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:sectionID];
  [model setHeader:[self headerForSection:sectionID]
      forSectionWithIdentifier:sectionID];
  for (TableViewItem<ReadingListListItem>* item in items) {
    item.type = ItemTypeItem;
    [self.dataSource fetchFaviconForItem:item];
    [model addItem:item toSectionWithIdentifier:sectionID];
  }
}

// Returns a TableViewTextItem that displays the title for the section
// designated by |sectionID|.
- (TableViewHeaderFooterItem*)headerForSection:(SectionIdentifier)sectionID {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];

  switch (sectionID) {
    case SectionIdentifierRead:
      header.text = l10n_util::GetNSString(IDS_IOS_READING_LIST_READ_HEADER);
      break;
    case SectionIdentifierUnread:
      header.text = l10n_util::GetNSString(IDS_IOS_READING_LIST_UNREAD_HEADER);
      break;
  }
  return header;
}

#pragma mark - Toolbar Helpers

// Updates buttons displayed in the bottom toolbar.
- (void)updateToolbarItems {
  self.toolbarManager.editing = self.tableView.editing;
  self.toolbarManager.hasReadItems =
      self.dataSource.hasElements && self.dataSource.hasReadElements;
  self.toolbarManager.selectionState = GetSelectionStateForSelectedCounts(
      self.selectedUnreadItemCount, self.selectedReadItemCount);
  if (self.toolbarManager.buttonItemsUpdated)
    [self setToolbarItems:[self.toolbarManager buttonItems] animated:YES];
  [self.toolbarManager updateMarkButtonTitle];
}

#pragma mark - Item Editing Helpers

// Returns |item| cast as a TableViewItem.
- (TableViewItem<ReadingListListItem>*)tableItemForReadingListItem:
    (id<ReadingListListItem>)item {
  return base::mac::ObjCCastStrict<TableViewItem<ReadingListListItem>>(item);
}

// Applies |updater| to the items in |section|. The updates are done in reverse
// order of the cells in the section to keep the order. Monitoring of the
// data source updates are suspended during this time.
- (void)updateItemsInSection:(SectionIdentifier)section
             withItemUpdater:(ReadingListListItemUpdater)updater {
  DCHECK(updater);
  [self.dataSource beginBatchUpdates];
  NSArray* items = [self.tableViewModel itemsInSectionWithIdentifier:section];
  // Read the objects in reverse order to keep the order (last modified first).
  for (id<ReadingListListItem> item in [items reverseObjectEnumerator]) {
    updater(item);
  }
  [self.dataSource endBatchUpdates];
}

// Applies |updater| to the items in |indexPaths|. The updates are done in
// reverse order |indexPaths| to keep the order. The monitoring of the data
// source updates are suspended during this time.
- (void)updateItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
                withItemUpdater:(ReadingListListItemUpdater)updater {
  DCHECK(updater);
  [self.dataSource beginBatchUpdates];
  // Read the objects in reverse order to keep the order (last modified first).
  for (NSIndexPath* indexPath in [indexPaths reverseObjectEnumerator]) {
    updater([self.tableViewModel itemAtIndexPath:indexPath]);
  }
  [self.dataSource endBatchUpdates];
}

// Moves all the items from |fromSection| to |toSection| and removes the empty
// section from the collection.
- (void)moveItemsFromSection:(SectionIdentifier)fromSection
                   toSection:(SectionIdentifier)toSection {
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

// Moves the items at |sortedIndexPaths| to |toSection|, removing any empty
// sections.
- (void)moveItemsAtIndexPaths:(NSArray*)sortedIndexPaths
                    toSection:(SectionIdentifier)toSection {
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
      // The |sortedIndexPaths| is a copy of the index paths before the
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
  [self removeEmptySections];
}

// Moves the ListItem within self.tableViewModel at |modelIndex| and the
// UITableViewCell at |tableViewIndex| to |toIndexPath|.
- (void)moveItemWithModelIndex:(NSIndexPath*)modelIndex
                tableViewIndex:(NSIndexPath*)tableViewIndex
                       toIndex:(NSIndexPath*)toIndexPath {
  TableViewModel* model = self.tableViewModel;
  TableViewItem* item = [model itemAtIndexPath:modelIndex];

  // Move the item in |model|.
  [self deleteItemAtIndexPathFromModel:modelIndex];
  NSInteger toSectionID =
      [model sectionIdentifierForSection:toIndexPath.section];
  [model insertItem:item
      inSectionWithIdentifier:toSectionID
                      atIndex:toIndexPath.row];

  // Move the cells in the table view.
  [self.tableView moveRowAtIndexPath:tableViewIndex toIndexPath:toIndexPath];
}

// Makes sure the table view section with |sectionID| exists with the correct
// header. Returns the index of the new section in the table view, or
// NSIntegerMax if no section has been created.
- (NSInteger)initializeTableViewSection:(SectionIdentifier)sectionID {
  TableViewModel* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionID])
    return NSIntegerMax;

  // There are at most two sections in the table.  The only time this creation
  // will result in the index of 1 is while creating the read section when there
  // are also unread items.
  BOOL hasUnreadItems = [self hasItemInSection:SectionIdentifierUnread];
  BOOL creatingReadSection = (sectionID == SectionIdentifierRead);
  NSInteger sectionIndex = (hasUnreadItems && creatingReadSection) ? 1 : 0;

  void (^updates)(void) = ^{
    [model insertSectionWithIdentifier:sectionID atIndex:sectionIndex];
    [model setHeader:[self headerForSection:sectionID]
        forSectionWithIdentifier:sectionID];
    [self.tableView insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]
                  withRowAnimation:UITableViewRowAnimationMiddle];
  };
  [self performBatchTableViewUpdates:updates completion:nil];

  return sectionIndex;
}

// Whether the model has items in |sectionID|.
- (BOOL)hasItemInSection:(SectionIdentifier)sectionID {
  return [self itemsForSection:sectionID].count > 0;
}

// Deletes the items at |indexPaths|, exiting editing and removing empty
// sections upon completion.
- (void)deleteItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths {
  [self deleteItemsAtIndexPaths:indexPaths
                     endEditing:YES
            removeEmptySections:YES];
}

// Deletes the items at |indexPaths|.  Exits editing mode if |endEditing| is
// YES.  Removes empty sections upon completion if |removeEmptySections| is YES.
- (void)deleteItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
                     endEditing:(BOOL)endEditing
            removeEmptySections:(BOOL)removeEmptySections {
  // Delete the items in the data source and exit editing mode.
  ReadingListListItemUpdater updater = ^(id<ReadingListListItem> item) {
    [self.dataSource removeEntryFromItem:item];
  };
  [self updateItemsAtIndexPaths:indexPaths withItemUpdater:updater];
  if (endEditing)
    [self exitEditingModeAnimated:YES];

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
}

// Deletes the ListItem corresponding to |indexPath| in the model.
- (void)deleteItemAtIndexPathFromModel:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger sectionID = [model sectionIdentifierForSection:indexPath.section];
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  NSUInteger index = [model indexInItemTypeForIndexPath:indexPath];
  [model removeItemWithType:itemType
      fromSectionWithIdentifier:sectionID
                        atIndex:index];
}

// Marks all the items at |indexPaths| as read or unread depending on |read|.
- (void)markItemsAtIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
               withReadStatus:(BOOL)read {
  // Record metric.
  base::RecordAction(base::UserMetricsAction(
      read ? "MobileReadingListMarkRead" : "MobileReadingListMarkUnread"));

  // Mark the items as |read| and exit editing.
  ReadingListListItemUpdater updater = ^(id<ReadingListListItem> item) {
    [self.dataSource setReadStatus:read forItem:item];
  };
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  [self updateItemsAtIndexPaths:sortedIndexPaths withItemUpdater:updater];
  [self exitEditingModeAnimated:YES];

  // Move the items to the appropriate section.
  SectionIdentifier toSection =
      read ? SectionIdentifierRead : SectionIdentifierUnread;
  [self moveItemsAtIndexPaths:sortedIndexPaths toSection:toSection];
}

// Marks items from |section| with as read or unread dending on |read|.
- (void)markItemsInSection:(SectionIdentifier)section
            withReadStatus:(BOOL)read {
  if (![self.tableViewModel hasSectionForSectionIdentifier:section]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  // Mark the items as |read| and exit editing.
  ReadingListListItemUpdater updater = ^(id<ReadingListListItem> item) {
    [self.dataSource setReadStatus:read forItem:item];
  };
  [self updateItemsInSection:section withItemUpdater:updater];
  [self exitEditingModeAnimated:YES];

  // Move the items to the appropriate section.
  SectionIdentifier toSection =
      read ? SectionIdentifierRead : SectionIdentifierUnread;
  [self moveItemsFromSection:section toSection:toSection];
}

// Cleanup function called in the completion block of editing operations.
- (void)batchEditDidFinish {
  // Reload the items if the datasource was modified during the edit.
  if (self.dataSourceModifiedWhileEditing)
    [self reloadData];
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
    SectionIdentifier sections[] = {SectionIdentifierRead,
                                    SectionIdentifierUnread};
    for (size_t i = 0; i < base::size(sections); ++i) {
      SectionIdentifier section = sections[i];

      if ([model hasSectionForSectionIdentifier:section] &&
          ![self hasItemInSection:section]) {
        // If |section| has no items, remove it from the model and the table
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

#pragma mark - Emtpy Table Helpers

// Called when the table is empty.
- (void)tableIsEmpty {
  UIImage* emptyImage = [[UIImage imageNamed:kEmptyStateImage]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [self addEmptyTableViewWithAttributedMessage:GetReadingListEmptyMessage()
                                         image:emptyImage];
  [self updateEmptyTableViewMessageAccessibilityLabel:
            GetReadingListEmptyMessageA11yLabel()];
  self.tableView.alwaysBounceVertical = NO;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  [self.audience readingListHasItems:NO];
}

#pragma mark - Gesture Helpers

// Shows the context menu for a long press from |recognizer|.
- (void)handleLongPress:(UILongPressGestureRecognizer*)recognizer {
  if (self.editing || recognizer.state != UIGestureRecognizerStateBegan)
    return;

  CGPoint location = [recognizer locationOfTouch:0 inView:self.tableView];
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:location];
  if (!indexPath)
    return;

  if (![self.tableViewModel hasItemAtIndexPath:indexPath])
    return;

  TableViewItem<ReadingListListItem>* item =
      [self.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != ItemTypeItem)
    return;

  [self.delegate readingListListViewController:self
                     displayContextMenuForItem:item
                                       atPoint:location];
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate dismissReadingListListViewController:self];
  return YES;
}

@end
