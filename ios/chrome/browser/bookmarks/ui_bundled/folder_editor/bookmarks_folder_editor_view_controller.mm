// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_editor/bookmarks_folder_editor_view_controller.h"

#import <memory>
#import <set>

#import "base/apple/foundation_util.h"
#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_text_field_item.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
  // Model object for bookmarks.
  base::WeakPtr<bookmarks::BookmarkModel> _bookmarkModel;
  // Observer for `_bookmarkModel` changes.
  std::unique_ptr<BookmarkModelBridge> _modelBridge;
  // The authentication service.
  raw_ptr<AuthenticationService> _authService;
  raw_ptr<syncer::SyncService> _syncService;
  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  // The browser for this view controller.
  base::WeakPtr<Browser> _browser;
  // Parent folder to `_folder`. Should never be `nullptr`.
  raw_ptr<const BookmarkNode> _parentFolder;
  // If `_folderNode` is `nullptr`, the user is adding a new folder. Otherwise
  // the user is editing an existing folder.
  raw_ptr<const BookmarkNode> _folder;

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
  // Whether the user manually changed the folder. In which case it must be
  // saved as last used folder on "save".
  BOOL _manuallyChangedTheFolder;
}

#pragma mark - Initialization

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                           folderNode:(const BookmarkNode*)folder
                     parentFolderNode:(const BookmarkNode*)parentFolder
                authenticationService:(AuthenticationService*)authService
                          syncService:(syncer::SyncService*)syncService
                              browser:(Browser*)browser {
  DCHECK(bookmarkModel);
  DCHECK(bookmarkModel->loaded());
  DCHECK(parentFolder);
  if (folder) {
    DCHECK(!bookmarkModel->is_permanent_node(folder));
  }
  DCHECK(browser);

  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _bookmarkModel = bookmarkModel->AsWeakPtr();
    _modelBridge =
        std::make_unique<BookmarkModelBridge>(self, _bookmarkModel.get());
    _folder = folder;
    _parentFolder = parentFolder;
    _editingExistingFolder = _folder != nullptr;
    _browser = browser->AsWeakPtr();
    _authService = authService;
    _syncService = syncService;
    // Set up the bookmark model oberver.
    _syncObserverModelBridge =
        std::make_unique<SyncObserverBridge>(self, syncService);
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel.reset();
  _modelBridge.reset();
  _folder = nullptr;
  _parentFolder = nullptr;
  _authService = nullptr;
  _syncService = nullptr;
  _syncObserverModelBridge.reset();
  _titleItem.delegate = nil;
  [self dismissActionSheetCoordinator];
}

- (void)dealloc {
  DCHECK(!_parentFolder);
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
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleDefault];
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf dismiss];
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleDestructive];
  // IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  weakSelf.navigationItem.leftBarButtonItem.enabled = YES;
                  weakSelf.navigationItem.rightBarButtonItem.enabled = YES;
                  [weakSelf dismissActionSheetCoordinator];
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
  _manuallyChangedTheFolder = YES;
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
                              editedNodes, _bookmarkModel.get(), self.profile,
                              FROM_HERE)];
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
      std::vector<const BookmarkNode*> bookmarksVector{_folder};
      [self.snackbarCommandsHandler
          showSnackbarMessage:bookmark_utils_ios::MoveBookmarksWithUndoToast(
                                  bookmarksVector, _bookmarkModel.get(),
                                  _parentFolder, self.profile,
                                  _authService->GetWeakPtr(), _syncService)];
      // Move might change the pointer, grab the updated value.
      CHECK_EQ(bookmarksVector.size(), 1u);
      _folder = bookmarksVector[0];
    }
  } else {
    DCHECK(!_folder);
    _folder = _bookmarkModel->AddFolder(
        _parentFolder, _parentFolder->children().size(), folderTitle);
  }

  if (_manuallyChangedTheFolder) {
    BookmarkStorageType type = bookmark_utils_ios::GetBookmarkStorageType(
        _parentFolder, _bookmarkModel.get());
    SetLastUsedBookmarkFolder(self.profile->GetPrefs(), _parentFolder, type);
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

- (void)bookmarkModelLoaded {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED_IN_MIGRATION();
}

- (void)didChangeNode:(const BookmarkNode*)bookmarkNode {
  if (bookmarkNode == _parentFolder) {
    [self updateParentFolderState];
  }
}

- (void)didChangeChildrenForNode:(const BookmarkNode*)bookmarkNode {
  // No-op.
}

- (void)didMoveNode:(const BookmarkNode*)bookmarkNode
         fromParent:(const BookmarkNode*)oldParent
           toParent:(const BookmarkNode*)newParent {
  if (_ignoresOwnMove) {
    return;
  }
  if (bookmarkNode == _folder) {
    DCHECK(oldParent == _parentFolder);
    _parentFolder = newParent;
    [self updateParentFolderState];
  }
}

- (void)willDeleteNode:(const bookmarks::BookmarkNode*)node
            fromFolder:(const bookmarks::BookmarkNode*)folder {
  if (_folder->HasAncestor(node)) {
    _folder = nullptr;
    if (_ignoresOwnMove) {
      // `saveFolder` will dismiss this screen after finishing the move.
      return;
    }
    [self dismiss];
  } else if (_parentFolder->HasAncestor(node)) {
    // This might happen when the user has changed `_parentFolder` but has not
    // commited the changes by pressing done. And in the background the chosen
    // folder was deleted.
    //
    // In this case, fall back to the default folder, which is the mobile node
    // for the same storage type as before (local or account). With account
    // bookmarks, it is possible that permanent folders no longer exist (e.g.
    // the user signed out), which requires falling back to the local default.
    // back to the local model.
    if (_bookmarkModel->IsLocalOnlyNode(*_parentFolder) ||
        !_bookmarkModel->account_mobile_node() ||
        _bookmarkModel->account_mobile_node()->HasAncestor(node)) {
      _parentFolder = _bookmarkModel->mobile_node();
    } else {
      _parentFolder = _bookmarkModel->account_mobile_node();
    }

    [self updateParentFolderState];
  }
}

- (void)didDeleteNode:(const BookmarkNode*)node
           fromFolder:(const BookmarkNode*)folder {
  // No-op. Bookmark deletion handled in
  // `bookmarkModel:willDeleteNode:fromFolder:`
}

- (void)bookmarkModelRemovedAllNodes {
  // Nothing more to do.
}

- (void)bookmarkModelWillRemoveAllNodes {
  // The current node is going to be deleted.
  // Just close the view.
  [self dismiss];
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

// Returns the profile.
- (ProfileIOS*)profile {
  if (Browser* browser = _browser.get()) {
    return browser->GetProfile()->GetOriginalProfile();
  }
  return nullptr;
}

- (void)dismissActionSheetCoordinator {
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nil;
}

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

// Updates `_parentFolderItem` without updating the table view.
- (void)updateParentFolderItem {
  CHECK(_parentFolderItem);
  _parentFolderItem.title =
      bookmark_utils_ios::TitleForBookmarkNode(_parentFolder);
  _parentFolderItem.shouldDisplayCloudSlashIcon =
      _bookmarkModel->IsLocalOnlyNode(*_parentFolder) &&
      bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService);
}

// Updates `_parentFolderItem` and reload the table view cell.
- (void)updateParentFolderState {
  [self updateParentFolderItem];
  NSIndexPath* folderSelectionIndexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeParentFolder
                              sectionIdentifier:SectionIdentifierInfo];
  [self.tableView reloadRowsAtIndexPaths:@[ folderSelectionIndexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
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
  [self updateParentFolderItem];
  [self.tableViewModel addItem:_parentFolderItem
       toSectionWithIdentifier:SectionIdentifierInfo];
}

// Adds delete button at the bottom.
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
  [self updateParentFolderState];
}

@end
