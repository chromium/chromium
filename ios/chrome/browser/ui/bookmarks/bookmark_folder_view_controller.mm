// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_folder_item.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The estimated height of every folder cell.
const CGFloat kEstimatedFolderCellHeight = 48.0;

// Height of section headers/footers.
const CGFloat kSectionHeaderHeight = 8.0;
const CGFloat kSectionFooterHeight = 8.0;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAddFolder = kSectionIdentifierEnumZero,
  SectionIdentifierBookmarkFolders,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCreateNewFolder = kItemTypeEnumZero,
  ItemTypeBookmarkFolder,
};

}  // namespace

using bookmarks::BookmarkNode;

@interface BookmarkFolderViewController ()<
    BookmarkFolderEditorViewControllerDelegate,
    BookmarkModelBridgeObserver,
    UITableViewDataSource,
    UITableViewDelegate> {
  std::set<const BookmarkNode*> _editedNodes;
  std::vector<const BookmarkNode*> _folders;
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
}

// Should the controller setup Cancel and Done buttons instead of a back button.
@property(nonatomic, assign) BOOL allowsCancel;

// Should the controller setup a new-folder button.
@property(nonatomic, assign) BOOL allowsNewFolders;

// Reference to the main bookmark model.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

// The currently selected folder.
@property(nonatomic, readonly) const BookmarkNode* selectedFolder;

// The view controller to present when creating a new folder.
@property(nonatomic, strong)
    BookmarkFolderEditorViewController* folderAddController;

// A linear list of folders.
@property(nonatomic, assign, readonly)
    const std::vector<const BookmarkNode*>& folders;

// The dispatcher for this ViewController.
@property(nonatomic, readonly, weak) id<BrowserCommands> dispatcher;

// Reloads the model and the updates |self.tableView| to reflect any model
// changes.
- (void)reloadModel;

// Pushes on the navigation controller a view controller to create a new folder.
- (void)pushFolderAddViewController;

// Called when the user taps on a folder row. The cell is checked, the UI is
// locked so that the user can't interact with it, then the delegate is
// notified. Usual implementations of this delegate callback are to pop or
// dismiss this controller on selection. The delay is here to let the user get a
// visual feedback of the selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection;

@end

@implementation BookmarkFolderViewController

@synthesize allowsCancel = _allowsCancel;
@synthesize allowsNewFolders = _allowsNewFolders;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize editedNodes = _editedNodes;
@synthesize folderAddController = _folderAddController;
@synthesize delegate = _delegate;
@synthesize folders = _folders;
@synthesize selectedFolder = _selectedFolder;

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                     allowsNewFolders:(BOOL)allowsNewFolders
                          editedNodes:
                              (const std::set<const BookmarkNode*>&)nodes
                         allowsCancel:(BOOL)allowsCancel
                       selectedFolder:(const BookmarkNode*)selectedFolder
                           dispatcher:(id<BrowserCommands>)dispatcher {
  DCHECK(bookmarkModel);
  DCHECK(bookmarkModel->loaded());
  DCHECK(selectedFolder == NULL || selectedFolder->is_folder());
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    _allowsCancel = allowsCancel;
    _allowsNewFolders = allowsNewFolders;
    _bookmarkModel = bookmarkModel;
    _editedNodes = nodes;
    _selectedFolder = selectedFolder;
    _dispatcher = dispatcher;

    // Set up the bookmark model oberver.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));
  }
  return self;
}

- (void)changeSelectedFolder:(const BookmarkNode*)selectedFolder {
  DCHECK(selectedFolder);
  DCHECK(selectedFolder->is_folder());
  _selectedFolder = selectedFolder;
  [self reloadModel];
}

- (void)dealloc {
  _folderAddController.delegate = nil;
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];

  self.view.accessibilityIdentifier =
      kBookmarkFolderPickerViewContainerIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_CHOOSE_GROUP_BUTTON);

  if (self.allowsCancel) {
    UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(cancel:)];
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;
  } else {
    UIBarButtonItem* backItem =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:self
                                            action:@selector(back:)];
    backItem.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BACK_LABEL);
    backItem.accessibilityIdentifier = @"Back";
    self.navigationItem.leftBarButtonItem = backItem;
  }

  // Configure the table view.
  self.tableView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  // Add a tableFooterView in order to disable separators at the bottom of the
  // tableView.
  self.tableView.tableFooterView = [[UIView alloc] init];

  self.tableView.estimatedRowHeight = kEstimatedFolderCellHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be hidden.
  self.navigationController.toolbarHidden = YES;

  // Load the model.
  [self reloadModel];
}

#pragma mark - Presentation controller integration

- (BOOL)shouldBeDismissedOnTouchOutside {
  return NO;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate folderPickerDidCancel:self];
  return YES;
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return kSectionHeaderHeight;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  CGRect headerViewFrame =
      CGRectMake(0, 0, CGRectGetWidth(tableView.frame),
                 [self tableView:tableView heightForHeaderInSection:section]);
  UIView* headerView = [[UIView alloc] initWithFrame:headerViewFrame];
  if (section ==
          [self.tableViewModel
              sectionForSectionIdentifier:SectionIdentifierBookmarkFolders] &&
      [self shouldShowDefaultSection]) {
    CGRect separatorFrame =
        CGRectMake(0, 0, CGRectGetWidth(headerView.bounds),
                   1.0 / [[UIScreen mainScreen] scale]);  // 1-pixel divider.
    UIView* separator = [[UIView alloc] initWithFrame:separatorFrame];
    separator.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin |
                                 UIViewAutoresizingFlexibleWidth;
    separator.backgroundColor = UIColor.cr_opaqueSeparatorColor;
    [headerView addSubview:separator];
  }
  return headerView;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return kSectionFooterHeight;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  return [[UIView alloc] init];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  switch ([self.tableViewModel sectionIdentifierForSection:indexPath.section]) {
    case SectionIdentifierAddFolder:
      [self pushFolderAddViewController];
      break;

    case SectionIdentifierBookmarkFolders: {
      int folderIndex = indexPath.row;
      // If |shouldShowDefaultSection| is YES, the first cell on this section
      // should call |pushFolderAddViewController|.
      if ([self shouldShowDefaultSection]) {
        NSInteger itemType =
            [self.tableViewModel itemTypeForIndexPath:indexPath];
        if (itemType == ItemTypeCreateNewFolder) {
          [self pushFolderAddViewController];
          return;
        }
        // If |shouldShowDefaultSection| is YES we need to offset by 1 to get
        // the right BookmarkNode from |self.folders|.
        folderIndex--;
      }
      const BookmarkNode* folder = self.folders[folderIndex];
      [self changeSelectedFolder:folder];
      [self delayedNotifyDelegateOfSelection];
      break;
    }
  }
}

#pragma mark - BookmarkFolderEditorViewControllerDelegate

- (void)bookmarkFolderEditor:(BookmarkFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const BookmarkNode*)folder {
  DCHECK(folder);
  [self reloadModel];
  [self changeSelectedFolder:folder];
  [self delayedNotifyDelegateOfSelection];
}

- (void)bookmarkFolderEditorDidDeleteEditedFolder:
    (BookmarkFolderEditorViewController*)folderEditor {
  NOTREACHED();
}

- (void)bookmarkFolderEditorDidCancel:
    (BookmarkFolderEditorViewController*)folderEditor {
  [self.navigationController popViewControllerAnimated:YES];
  self.folderAddController.delegate = nil;
  self.folderAddController = nil;
}

- (void)bookmarkFolderEditorWillCommitTitleChange:
    (BookmarkFolderEditorViewController*)controller {
  // Do nothing.
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate folderPickerDidDismiss:self];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (!bookmarkNode->is_folder())
    return;
  [self reloadModel];
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  [self reloadModel];
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (bookmarkNode->is_folder()) {
    [self reloadModel];
  }
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode
                 fromFolder:(const BookmarkNode*)folder {
  // Remove node from editedNodes if it is already deleted (possibly remotely by
  // another sync device).
  if (self.editedNodes.find(bookmarkNode) != self.editedNodes.end()) {
    self.editedNodes.erase(bookmarkNode);
    // if editedNodes becomes empty, nothing to move.  Exit the folder picker.
    if (self.editedNodes.empty()) {
      [self.delegate folderPickerDidCancel:self];
    }
    // Exit here because nodes in editedNodes cannot be any visible folders in
    // folder picker.
    return;
  }

  if (!bookmarkNode->is_folder())
    return;

  if (bookmarkNode == self.selectedFolder) {
    // The selected folder has been deleted. Fallback on the Mobile Bookmarks
    // node.
    [self changeSelectedFolder:self.bookmarkModel->mobile_node()];
  }
  [self reloadModel];
}

- (void)bookmarkModelRemovedAllNodes {
  // The selected folder is no longer valid. Fallback on the Mobile Bookmarks
  // node.
  [self changeSelectedFolder:self.bookmarkModel->mobile_node()];
  [self reloadModel];
}

#pragma mark - Actions

- (void)done:(id)sender {
  [self.delegate folderPicker:self didFinishWithFolder:self.selectedFolder];
}

- (void)cancel:(id)sender {
  [self.delegate folderPickerDidCancel:self];
}

- (void)back:(id)sender {
  [self.delegate folderPickerDidCancel:self];
}

#pragma mark - Private

- (BOOL)shouldShowDefaultSection {
  return self.allowsNewFolders;
}

- (void)reloadModel {
  _folders = bookmark_utils_ios::VisibleNonDescendantNodes(self.editedNodes,
                                                           self.bookmarkModel);

  // Delete any existing section.
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierAddFolder])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierAddFolder];
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierBookmarkFolders])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierBookmarkFolders];

  // Creates Folders Section
  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierBookmarkFolders];

  // Adds default "Add Folder" item if needed.
  if ([self shouldShowDefaultSection]) {
    BookmarkFolderItem* createFolderItem =
        [[BookmarkFolderItem alloc] initWithType:ItemTypeCreateNewFolder
                                           style:BookmarkFolderStyleNewFolder];
    // Add the "Add Folder" Item to the same section as the rest of the folder
    // entries.
    [self.tableViewModel addItem:createFolderItem
         toSectionWithIdentifier:SectionIdentifierBookmarkFolders];
  }

  // Add Folders entries.
  for (NSUInteger row = 0; row < _folders.size(); row++) {
    const BookmarkNode* folderNode = self.folders[row];
    BookmarkFolderItem* folderItem = [[BookmarkFolderItem alloc]
        initWithType:ItemTypeBookmarkFolder
               style:BookmarkFolderStyleFolderEntry];
    folderItem.title = bookmark_utils_ios::TitleForBookmarkNode(folderNode);
    folderItem.currentFolder = (self.selectedFolder == folderNode);

    // Indentation level.
    NSInteger level = 0;
    const BookmarkNode* node = folderNode;
    while (node && !(self.bookmarkModel->is_root_node(node))) {
      ++level;
      node = node->parent();
    }
    // The root node is not shown as a folder, so top level folders have a
    // level strictly positive.
    DCHECK(level > 0);
    folderItem.indentationLevel = level - 1;

    [self.tableViewModel addItem:folderItem
         toSectionWithIdentifier:SectionIdentifierBookmarkFolders];
  }

  [self.tableView reloadData];
}

- (void)pushFolderAddViewController {
  DCHECK(self.allowsNewFolders);
  BookmarkFolderEditorViewController* folderCreator =
      [BookmarkFolderEditorViewController
          folderCreatorWithBookmarkModel:self.bookmarkModel
                            parentFolder:self.selectedFolder
                              dispatcher:self.dispatcher];
  folderCreator.delegate = self;
  [self.navigationController pushViewController:folderCreator animated:YES];
  self.folderAddController = folderCreator;
}

- (void)delayedNotifyDelegateOfSelection {
  self.view.userInteractionEnabled = NO;
  __weak BookmarkFolderViewController* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        BookmarkFolderViewController* strongSelf = weakSelf;
        // Early return if the controller has been deallocated.
        if (!strongSelf)
          return;
        strongSelf.view.userInteractionEnabled = YES;
        [strongSelf done:nil];
      });
}

@end
