// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_view_controller.h"

#import <memory>
#import <set>

#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInfo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFolderTitle = kItemTypeEnumZero,
  ItemTypeParentFolder,
};

}  // namespace

@interface BookmarksFolderEditorViewController () <
    BookmarkModelBridgeObserver,
    BookmarkTextFieldItemDelegate,
    SyncObserverModelBridge>
@end

@implementation BookmarksFolderEditorViewController {
  bookmarks::BookmarkModel* _bookmarkModel;
  std::unique_ptr<BookmarkModelBridge> _modelBridge;
  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  SyncSetupService* _syncSetupService;
  // The browser for this view controller.
  base::WeakPtr<Browser> _browser;
  ChromeBrowserState* _browserState;
  const BookmarkNode* _parentFolder;
  const BookmarkNode* _folder;

  BOOL _edited;
  BOOL _editingExistingFolder;
  // Flag to ignore bookmark model Move notifications when the move is performed
  // by this class.
  BOOL _ignoresOwnMove;
  __weak UIBarButtonItem* _doneItem;
  __strong BookmarkTextFieldItem* _titleItem;
  __strong BookmarkParentFolderItem* _parentFolderItem;
  // The action sheet coordinator, if one is currently being shown.
  __strong ActionSheetCoordinator* _actionSheetCoordinator;
}

#pragma mark - Initialization

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                           folderNode:(const BookmarkNode*)folder
                     parentFolderNode:(const BookmarkNode*)parentFolder
                     syncSetupService:(SyncSetupService*)syncSetupService
                          syncService:(syncer::SyncService*)syncService
                              browser:(Browser*)browser {
  DCHECK(bookmarkModel);
  DCHECK(bookmarkModel->loaded());
  // Both of these can't be `nullptr`.
  DCHECK(parentFolder || folder)
      << "parentFolder: " << parentFolder << ", folder: " << folder;
  if (folder) {
    DCHECK(!bookmarkModel->is_permanent_node(folder));
  }
  DCHECK(browser);

  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _bookmarkModel = bookmarkModel;
    _folder = folder;
    _parentFolder = parentFolder ? parentFolder : _folder->parent();
    _editingExistingFolder = _folder != nullptr;
    _browser = browser->AsWeakPtr();
    _browserState = browser->GetBrowserState()->GetOriginalChromeBrowserState();
    // Set up the bookmark model oberver.
    _modelBridge.reset(new BookmarkModelBridge(self, _bookmarkModel));
    _syncObserverModelBridge.reset(new SyncObserverBridge(self, syncService));
    _syncSetupService = syncSetupService;
  }
  return self;
}

- (void)disconnect {
  _browserState = nullptr;
  _bookmarkModel = nullptr;
  _modelBridge = nullptr;
  _folder = nullptr;
  _parentFolder = nullptr;
  _syncObserverModelBridge = nullptr;
  _syncSetupService = nullptr;
  _titleItem.delegate = nil;
}

#pragma mark - Public

- (void)presentationControllerDidAttemptToDismiss {
  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser.get()
                           title:nil
                         message:nil
                   barButtonItem:self.navigationItem.leftBarButtonItem];

  __weak __typeof(self) weakSelf = self;
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES)
                action:^{
                  [weakSelf saveFolder];
                }
                 style:UIAlertActionStyleDefault];
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf dismiss];
                }
                 style:UIAlertActionStyleDestructive];
  // IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  weakSelf.navigationItem.leftBarButtonItem.enabled = YES;
                  weakSelf.navigationItem.rightBarButtonItem.enabled = YES;
                }
                 style:UIAlertActionStyleCancel];

  self.navigationItem.leftBarButtonItem.enabled = NO;
  self.navigationItem.rightBarButtonItem.enabled = NO;
  [_actionSheetCoordinator start];
}

// Whether the bookmarks folder editor can be dismissed.
- (BOOL)canDismiss {
  return !_edited;
}

- (void)updateParentFolder:(const BookmarkNode*)parent {
  DCHECK(parent);
  _parentFolder = parent;
  [self updateParentFolderState];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor = self.styler.tableViewBackgroundColor;
  self.tableView.estimatedRowHeight = 150.0;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kBookmarkCellHorizontalLeadingInset,
                                         0, 0)];

  // Add Done button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(saveFolder)];
  doneItem.accessibilityIdentifier =
      kBookmarkFolderEditNavigationBarDoneButtonIdentifier;
  self.navigationItem.rightBarButtonItem = doneItem;
  _doneItem = doneItem;

  if (_editingExistingFolder) {
    // Add Cancel Button.
    UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(dismiss)];
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;

    [self addToolbar];
  }
  [self updateEditingState];
  [self setupCollectionViewModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateSaveButtonState];
  if (_editingExistingFolder) {
    self.navigationController.toolbarHidden = NO;
  } else {
    self.navigationController.toolbarHidden = YES;
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate bookmarksFolderEditorDidDismiss:self];
  }
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self dismiss];
  return YES;
}

#pragma mark - Actions

- (void)dismiss {
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksFolderEditorCanceled"));
  [self.view endEditing:YES];
  [self.delegate bookmarksFolderEditorDidCancel:self];
}

- (void)deleteFolder {
  DCHECK(_editingExistingFolder);
  DCHECK(_folder);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksFolderEditorDeletedFolder"));
  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(_folder);
  [self.snackbarCommandsHandler
      showSnackbarMessage:bookmark_utils_ios::DeleteBookmarksWithUndoToast(
                              editedNodes, _bookmarkModel, _browserState)];
  [self.delegate bookmarksFolderEditorDidDeleteEditedFolder:self];
}

- (void)saveFolder {
  DCHECK(_parentFolder);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksFolderEditorSaved"));
  NSString* folderString = _titleItem.text;
  DCHECK(folderString.length > 0);
  std::u16string folderTitle = base::SysNSStringToUTF16(folderString);

  if (_editingExistingFolder) {
    DCHECK(_folder);
    // Tell delegate if folder title has been changed.
    if (_folder->GetTitle() != folderTitle) {
      [self.delegate bookmarksFolderEditorWillCommitTitleChange:self];
    }

    _bookmarkModel->SetTitle(_folder, folderTitle,
                             bookmarks::metrics::BookmarkEditSource::kUser);
    if (_folder->parent() != _parentFolder) {
      base::AutoReset<BOOL> autoReset(&_ignoresOwnMove, YES);
      [self.snackbarCommandsHandler
          showSnackbarMessage:bookmark_utils_ios::MoveBookmarksWithUndoToast(
                                  std::set<const BookmarkNode*>{_folder},
                                  _bookmarkModel, _parentFolder,
                                  _browserState)];
    }
  } else {
    DCHECK(!_folder);
    _folder = _bookmarkModel->AddFolder(
        _parentFolder, _parentFolder->children().size(), folderTitle);
  }
  [self.view endEditing:YES];
  [self.delegate bookmarksFolderEditor:self didFinishEditingFolder:_folder];
}

- (void)changeParentFolder {
  base::RecordAction(base::UserMetricsAction(
      "MobileBookmarksFolderEditorOpenedFolderChooser"));
  std::set<const BookmarkNode*> hiddenNodes;
  if (_folder) {
    hiddenNodes.insert(_folder);
  }
  [self.delegate showBookmarksFolderChooserWithParentFolder:_parentFolder
                                                hiddenNodes:hiddenNodes];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded:(bookmarks::BookmarkModel*)model {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (bookmarkNode == _parentFolder) {
    [self updateParentFolderState];
  }
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  // No-op.
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  if (_ignoresOwnMove) {
    return;
  }
  if (bookmarkNode == _folder) {
    DCHECK(oldParent == _parentFolder);
    _parentFolder = newParent;
    [self updateParentFolderState];
  }
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  if (node == _parentFolder) {
    _parentFolder = NULL;
    [self updateParentFolderState];
    return;
  }
  if (node == _folder) {
    _folder = NULL;
    _editingExistingFolder = NO;
    [self updateEditingState];
  }
}

- (void)bookmarkModelRemovedAllNodes:(bookmarks::BookmarkModel*)model {
  if (_bookmarkModel->is_permanent_node(_parentFolder)) {
    return;  // The current parent folder is still valid.
  }

  _parentFolder = NULL;
  [self updateParentFolderState];
}

#pragma mark - BookmarkTextFieldItemDelegate

- (void)textDidChangeForItem:(BookmarkTextFieldItem*)item {
  _edited = YES;
  [self updateSaveButtonState];
}

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  textField.textColor = [BookmarkTextFieldCell textColorForEditing:YES];
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  textField.textColor = [BookmarkTextFieldCell textColorForEditing:NO];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeParentFolder) {
    [self changeParentFolder];
  }
}

#pragma mark - Private

- (void)updateEditingState {
  if (![self isViewLoaded]) {
    return;
  }

  self.view.accessibilityIdentifier =
      (_folder) ? kBookmarkFolderEditViewContainerIdentifier
                : kBookmarkFolderCreateViewContainerIdentifier;

  [self setTitle:(_folder)
                     ? l10n_util::GetNSString(
                           IDS_IOS_BOOKMARK_NEW_GROUP_EDITOR_EDIT_TITLE)
                     : l10n_util::GetNSString(
                           IDS_IOS_BOOKMARK_NEW_GROUP_EDITOR_CREATE_TITLE)];
}

- (void)updateParentFolderState {
  NSIndexPath* folderSelectionIndexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeParentFolder
                              sectionIdentifier:SectionIdentifierInfo];
  _parentFolderItem.title =
      bookmark_utils_ios::TitleForBookmarkNode(_parentFolder);
  _parentFolderItem.shouldDisplayCloudSlashIcon =
      bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
  [self.tableView reloadRowsAtIndexPaths:@[ folderSelectionIndexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];

  if (_editingExistingFolder && self.navigationController.isToolbarHidden) {
    [self addToolbar];
  }

  if (!_editingExistingFolder && !self.navigationController.isToolbarHidden) {
    self.navigationController.toolbarHidden = YES;
  }
}

- (void)setupCollectionViewModel {
  [self loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierInfo];

  _titleItem = [[BookmarkTextFieldItem alloc] initWithType:ItemTypeFolderTitle];
  _titleItem.text =
      (_folder)
          ? bookmark_utils_ios::TitleForBookmarkNode(_folder)
          : l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_GROUP_DEFAULT_NAME);
  _titleItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_EDITOR_NAME_LABEL);
  _titleItem.accessibilityIdentifier = @"Title";
  [self.tableViewModel addItem:_titleItem
       toSectionWithIdentifier:SectionIdentifierInfo];
  _titleItem.delegate = self;

  _parentFolderItem =
      [[BookmarkParentFolderItem alloc] initWithType:ItemTypeParentFolder];
  _parentFolderItem.title =
      bookmark_utils_ios::TitleForBookmarkNode(_parentFolder);
  _parentFolderItem.shouldDisplayCloudSlashIcon =
      bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
  [self.tableViewModel addItem:_parentFolderItem
       toSectionWithIdentifier:SectionIdentifierInfo];
}

- (void)addToolbar {
  self.navigationController.toolbarHidden = NO;
  NSString* titleString = l10n_util::GetNSString(IDS_IOS_BOOKMARK_GROUP_DELETE);
  UIBarButtonItem* deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(deleteFolder)];
  deleteButton.accessibilityIdentifier =
      kBookmarkFolderEditorDeleteButtonIdentifier;
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  deleteButton.tintColor = [UIColor colorNamed:kRedColor];

  [self setToolbarItems:@[ spaceButton, deleteButton, spaceButton ]
               animated:NO];
}

- (void)updateSaveButtonState {
  _doneItem.enabled = (_titleItem.text.length > 0);
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  _parentFolderItem.shouldDisplayCloudSlashIcon =
      bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeParentFolder
                              sectionIdentifier:SectionIdentifierInfo];
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];
}

@end
