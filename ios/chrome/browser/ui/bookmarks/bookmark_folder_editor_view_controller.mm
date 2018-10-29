// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"

#include <memory>
#include <set>

#include "base/auto_reset.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

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

@interface BookmarkFolderEditorViewController ()<
    BookmarkFolderViewControllerDelegate,
    BookmarkModelBridgeObserver,
    BookmarkTextFieldItemDelegate> {
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;

  // Flag to ignore bookmark model Move notifications when the move is performed
  // by this class.
  BOOL _ignoresOwnMove;
}
@property(nonatomic, assign) BOOL editingExistingFolder;
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
@property(nonatomic, assign) const BookmarkNode* folder;
@property(nonatomic, strong) BookmarkFolderViewController* folderViewController;
@property(nonatomic, assign) const BookmarkNode* parentFolder;
@property(nonatomic, weak) UIBarButtonItem* doneItem;
@property(nonatomic, strong) BookmarkTextFieldItem* titleItem;
@property(nonatomic, strong) BookmarkParentFolderItem* parentFolderItem;

// |bookmarkModel| must not be NULL and must be loaded.
- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
    NS_DESIGNATED_INITIALIZER;

// Enables or disables the save button depending on the state of the form.
- (void)updateSaveButtonState;

// Configures collection view model.
- (void)setupCollectionViewModel;

// Bottom toolbar with DELETE button that only appears when the edited folder
// allows deletion.
- (void)addToolbar;

@end

@implementation BookmarkFolderEditorViewController

@synthesize bookmarkModel = _bookmarkModel;
@synthesize delegate = _delegate;
@synthesize editingExistingFolder = _editingExistingFolder;
@synthesize folder = _folder;
@synthesize folderViewController = _folderViewController;
@synthesize parentFolder = _parentFolder;
@synthesize browserState = _browserState;
@synthesize doneItem = _doneItem;
@synthesize titleItem = _titleItem;
@synthesize parentFolderItem = _parentFolderItem;

#pragma mark - Class methods

+ (instancetype)
folderCreatorWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                  parentFolder:(const BookmarkNode*)parentFolder {
  BookmarkFolderEditorViewController* folderCreator =
      [[self alloc] initWithBookmarkModel:bookmarkModel];
  folderCreator.parentFolder = parentFolder;
  folderCreator.folder = NULL;
  folderCreator.editingExistingFolder = NO;
  return folderCreator;
}

+ (instancetype)
folderEditorWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                       folder:(const BookmarkNode*)folder
                 browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(folder);
  DCHECK(!bookmarkModel->is_permanent_node(folder));
  DCHECK(browserState);
  BookmarkFolderEditorViewController* folderEditor =
      [[self alloc] initWithBookmarkModel:bookmarkModel];
  folderEditor.parentFolder = folder->parent();
  folderEditor.folder = folder;
  folderEditor.browserState = browserState;
  folderEditor.editingExistingFolder = YES;
  return folderEditor;
}

#pragma mark - Initialization

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  DCHECK(bookmarkModel);
  DCHECK(bookmarkModel->loaded());
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _bookmarkModel = bookmarkModel;

    // Set up the bookmark model oberver.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));
  }
  return self;
}

- (void)dealloc {
  _titleItem.delegate = nil;
  _folderViewController.delegate = nil;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor = self.styler.tableViewBackgroundColor;
  self.tableView.estimatedRowHeight = 150.0;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  self.tableView.tableFooterView = [[UIView alloc] init];
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kBookmarkCellHorizontalLeadingInset,
                                         0, 0)];

  // Add Done button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_BOOKMARK_EDIT_MODE_EXIT_MOBILE)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(saveFolder)];
  doneItem.accessibilityIdentifier =
      kBookmarkFolderEditNavigationBarDoneButtonIdentifier;
  self.navigationItem.rightBarButtonItem = doneItem;
  self.doneItem = doneItem;

  if (self.editingExistingFolder) {
    // Add Cancel Button.
    UIBarButtonItem* cancelItem =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon closeIcon]
                                            target:self
                                            action:@selector(cancel)];
    cancelItem.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_CANCEL_BUTTON_LABEL);
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;

    [self addToolbar];
  } else {
    // Add Back button.
    UIBarButtonItem* backItem =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:self
                                            action:@selector(back)];
    backItem.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BACK_LABEL);
    backItem.accessibilityIdentifier = @"Back";
    self.navigationItem.leftBarButtonItem = backItem;
  }
  [self updateEditingState];
  [self setupCollectionViewModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateSaveButtonState];
  if (self.editingExistingFolder) {
    self.navigationController.toolbarHidden = NO;
  } else {
    self.navigationController.toolbarHidden = YES;
  }
}

#pragma mark - Presentation controller integration

- (BOOL)shouldBeDismissedOnTouchOutside {
  return NO;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate bookmarkFolderEditorDidCancel:self];
  return YES;
}

#pragma mark - Actions

- (void)back {
  [self.delegate bookmarkFolderEditorDidCancel:self];
}

- (void)cancel {
  [self.delegate bookmarkFolderEditorDidCancel:self];
}

- (void)deleteFolder {
  DCHECK(self.editingExistingFolder);
  DCHECK(self.folder);
  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(self.folder);
  bookmark_utils_ios::DeleteBookmarksWithUndoToast(
      editedNodes, self.bookmarkModel, self.browserState);
  [self.delegate bookmarkFolderEditorDidDeleteEditedFolder:self];
}

- (void)saveFolder {
  DCHECK(self.parentFolder);

  NSString* folderString = self.titleItem.text;
  DCHECK(folderString.length > 0);
  base::string16 folderTitle = base::SysNSStringToUTF16(folderString);

  if (self.editingExistingFolder) {
    DCHECK(self.folder);
    // Tell delegate if folder title has been changed.
    if (self.folder->GetTitle() != folderTitle) {
      [self.delegate bookmarkFolderEditorWillCommitTitleChange:self];
    }

    self.bookmarkModel->SetTitle(self.folder, folderTitle);
    if (self.folder->parent() != self.parentFolder) {
      base::AutoReset<BOOL> autoReset(&_ignoresOwnMove, YES);
      std::set<const BookmarkNode*> editedNodes;
      editedNodes.insert(self.folder);
      bookmark_utils_ios::MoveBookmarksWithUndoToast(
          editedNodes, self.bookmarkModel, self.parentFolder,
          self.browserState);
    }
  } else {
    DCHECK(!self.folder);
    self.folder = self.bookmarkModel->AddFolder(
        self.parentFolder, self.parentFolder->child_count(), folderTitle);
  }
  [self.delegate bookmarkFolderEditor:self didFinishEditingFolder:self.folder];
}

- (void)changeParentFolder {
  std::set<const BookmarkNode*> editedNodes;
  if (self.folder)
    editedNodes.insert(self.folder);
  BookmarkFolderViewController* folderViewController =
      [[BookmarkFolderViewController alloc]
          initWithBookmarkModel:self.bookmarkModel
               allowsNewFolders:NO
                    editedNodes:editedNodes
                   allowsCancel:NO
                 selectedFolder:self.parentFolder];
  folderViewController.delegate = self;
  self.folderViewController = folderViewController;

  [self.navigationController pushViewController:folderViewController
                                       animated:YES];
}

#pragma mark - BookmarkFolderViewControllerDelegate

- (void)folderPicker:(BookmarkFolderViewController*)folderPicker
    didFinishWithFolder:(const BookmarkNode*)folder {
  self.parentFolder = folder;
  [self updateParentFolderState];
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidCancel:(BookmarkFolderViewController*)folderPicker {
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (bookmarkNode == self.parentFolder) {
    [self updateParentFolderState];
  }
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // No-op.
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (_ignoresOwnMove)
    return;
  if (bookmarkNode == self.folder) {
    DCHECK(oldParent == self.parentFolder);
    self.parentFolder = newParent;
    [self updateParentFolderState];
  }
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode
                 fromFolder:(const BookmarkNode*)folder {
  if (bookmarkNode == self.parentFolder) {
    self.parentFolder = NULL;
    [self updateParentFolderState];
    return;
  }
  if (bookmarkNode == self.folder) {
    self.folder = NULL;
    self.editingExistingFolder = NO;
    [self updateEditingState];
  }
}

- (void)bookmarkModelRemovedAllNodes {
  if (self.bookmarkModel->is_permanent_node(self.parentFolder))
    return;  // The current parent folder is still valid.

  self.parentFolder = NULL;
  [self updateParentFolderState];
}

#pragma mark - BookmarkTextFieldItemDelegate

- (void)textDidChangeForItem:(BookmarkTextFieldItem*)item {
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

- (void)setParentFolder:(const BookmarkNode*)parentFolder {
  if (!parentFolder) {
    parentFolder = self.bookmarkModel->mobile_node();
  }
  _parentFolder = parentFolder;
}

- (void)updateEditingState {
  if (![self isViewLoaded])
    return;

  self.view.accessibilityIdentifier =
      (self.folder) ? kBookmarkFolderEditViewContainerIdentifier
                    : kBookmarkFolderCreateViewContainerIdentifier;

  [self setTitle:(self.folder)
                     ? l10n_util::GetNSString(
                           IDS_IOS_BOOKMARK_NEW_GROUP_EDITOR_EDIT_TITLE)
                     : l10n_util::GetNSString(
                           IDS_IOS_BOOKMARK_NEW_GROUP_EDITOR_CREATE_TITLE)];
}

- (void)updateParentFolderState {
  NSIndexPath* folderSelectionIndexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeParentFolder
                              sectionIdentifier:SectionIdentifierInfo];
  self.parentFolderItem.title =
      bookmark_utils_ios::TitleForBookmarkNode(self.parentFolder);
  [self.tableView reloadRowsAtIndexPaths:@[ folderSelectionIndexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];

  if (self.editingExistingFolder && self.navigationController.isToolbarHidden)
    [self addToolbar];

  if (!self.editingExistingFolder && !self.navigationController.isToolbarHidden)
    self.navigationController.toolbarHidden = YES;
}

- (void)setupCollectionViewModel {
  [self loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierInfo];

  BookmarkTextFieldItem* titleItem =
      [[BookmarkTextFieldItem alloc] initWithType:ItemTypeFolderTitle];
  titleItem.text =
      (self.folder)
          ? bookmark_utils_ios::TitleForBookmarkNode(self.folder)
          : l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_GROUP_DEFAULT_NAME);
  titleItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_EDITOR_NAME_LABEL);
  titleItem.accessibilityIdentifier = @"Title";
  [self.tableViewModel addItem:titleItem
       toSectionWithIdentifier:SectionIdentifierInfo];
  titleItem.delegate = self;
  self.titleItem = titleItem;

  BookmarkParentFolderItem* parentFolderItem =
      [[BookmarkParentFolderItem alloc] initWithType:ItemTypeParentFolder];
  parentFolderItem.title =
      bookmark_utils_ios::TitleForBookmarkNode(self.parentFolder);
  [self.tableViewModel addItem:parentFolderItem
       toSectionWithIdentifier:SectionIdentifierInfo];
  self.parentFolderItem = parentFolderItem;
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
  deleteButton.tintColor = [UIColor redColor];
  [self.navigationController.toolbar setShadowImage:[UIImage new]
                                 forToolbarPosition:UIBarPositionAny];
  [self setToolbarItems:@[ spaceButton, deleteButton, spaceButton ]
               animated:NO];
}

- (void)updateSaveButtonState {
  self.doneItem.enabled = (self.titleItem.text.length > 0);
}

@end
