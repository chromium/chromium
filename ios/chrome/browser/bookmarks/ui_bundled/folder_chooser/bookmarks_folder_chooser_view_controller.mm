// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_view_controller.h"

#import <memory>
#import <vector>

#import "base/check.h"
#import "base/containers/contains.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/table_view_bookmarks_folder_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_mutator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_view_controller_presentation_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The estimated height of every folder cell.
const CGFloat kEstimatedFolderCellHeight = 48.0;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLocalOrSyncableBookmarks = kSectionIdentifierEnumZero,
  SectionIdentifierAccountBookmarks,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeCreateNewFolder,
  ItemTypeBookmarkFolder,
};

}  // namespace

using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserViewController () <UITableViewDataSource,
                                                    UITableViewDelegate>
@end

@implementation BookmarksFolderChooserViewController {
  // Should the controller setup Cancel and Done buttons instead of a back
  // button.
  BOOL _allowsCancel;
  // Should the controller setup a new-folder button.
  BOOL _allowsNewFolders;
  // A linear list of folders. This will be populated in `reloadView` when the
  // UI is updated.
  std::vector<const BookmarkNode*> _accountFolderNodes;
  // A linear list of folders. This will be populated in `reloadView` when the
  // UI is updated.
  std::vector<const BookmarkNode*> _localOrSyncableFolderNodes;
}

- (instancetype)initWithAllowsCancel:(BOOL)allowsCancel
                    allowsNewFolders:(BOOL)allowsNewFolders {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _allowsCancel = allowsCancel;
    _allowsNewFolders = allowsNewFolders;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];

  self.view.accessibilityIdentifier =
      kBookmarkFolderPickerViewContainerIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_CHOOSE_GROUP_BUTTON);

  if (_allowsCancel) {
    UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(cancel:)];
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;
  }
  // Configure the table view.
  self.tableView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  self.tableView.estimatedRowHeight = kEstimatedFolderCellHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be hidden.
  self.navigationController.toolbarHidden = YES;

  // Load the model.
  [self reloadView];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate bookmarksFolderChooserViewControllerDidDismiss:self];
  }
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate bookmarksFolderChooserViewControllerDidCancel:self];
  return YES;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  size_t folderIndex = indexPath.row;
  NSInteger sectionID =
      [self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section];
  // If new folders are allowed, the first cell on this section should call
  // `showBookmarksFolderEditor`.
  if (_allowsNewFolders) {
    NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
    if (itemType == ItemTypeCreateNewFolder) {
      // Set the 'Mobile Bookmarks' folder of the corresponding section to be
      // the parent folder.
      const BookmarkNode* parentNode = nullptr;
      if (!parentNode) {
        // If `parent` (selected folder) is `nullptr`, set the root folder of
        // the corresponding section to be the parent folder.
        parentNode =
            (sectionID == SectionIdentifierAccountBookmarks &&
             [self.dataSource.accountDataSource mobileFolderNode] != nullptr)
                ? [self.dataSource.accountDataSource mobileFolderNode]
                : [self.dataSource.localOrSyncableDataSource mobileFolderNode];
      }
      [self.delegate showBookmarksFolderEditorWithParentFolderNode:parentNode];
      return;
    }
    // If new folders are allowed, we need to offset by 1 to get the right
    // BookmarkNode from folders.
    DCHECK(folderIndex > 0);
    folderIndex--;
  }

  const BookmarkNode* folder;
  if (sectionID == SectionIdentifierAccountBookmarks) {
    DCHECK(folderIndex < _accountFolderNodes.size());
    folder = _accountFolderNodes[folderIndex];
  } else {
    DCHECK(folderIndex < _localOrSyncableFolderNodes.size());
    folder = _localOrSyncableFolderNodes[folderIndex];
  }
  [_mutator setSelectedFolderNode:folder];
  [self delayedNotifyDelegateOfSelection];
}

#pragma mark - BookmarksFolderChooserConsumer

- (void)notifyModelUpdated {
  [self reloadView];
}

#pragma mark - Actions

- (void)done:(id)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksFolderChooserDone"));
  [self.delegate
      bookmarksFolderChooserViewController:self
                       didFinishWithFolder:[self.dataSource
                                                   selectedFolderNode]];
}

- (void)cancel:(id)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksFolderChooserCanceled"));
  [self.delegate bookmarksFolderChooserViewControllerDidCancel:self];
}

#pragma mark - Private

- (void)reloadView {
  // Delete any existing section.
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierAccountBookmarks]) {
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierAccountBookmarks];
  }
  if ([self.tableViewModel hasSectionForSectionIdentifier:
                               SectionIdentifierLocalOrSyncableBookmarks]) {
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierLocalOrSyncableBookmarks];
  }

  if ([self.dataSource shouldShowAccountBookmarks]) {
    _accountFolderNodes =
        [self.dataSource.accountDataSource visibleFolderNodes];
    [self reloadSectionWithIdentifier:SectionIdentifierAccountBookmarks];
  }
  _localOrSyncableFolderNodes =
      [self.dataSource.localOrSyncableDataSource visibleFolderNodes];
  [self reloadSectionWithIdentifier:SectionIdentifierLocalOrSyncableBookmarks];
  if ([self.dataSource shouldShowAccountBookmarks]) {
    // The headers are only shown if both sections are visible.
    [self.tableViewModel setHeader:[self headerForSectionWithIdentifier:
                                             SectionIdentifierAccountBookmarks]
          forSectionWithIdentifier:SectionIdentifierAccountBookmarks];
    [self.tableViewModel
                       setHeader:
                           [self headerForSectionWithIdentifier:
                                     SectionIdentifierLocalOrSyncableBookmarks]
        forSectionWithIdentifier:SectionIdentifierLocalOrSyncableBookmarks];
  }
  [self.tableView reloadData];
}

- (void)reloadSectionWithIdentifier:(SectionIdentifier)sectionID {
  // Creates Folders Section
  [self.tableViewModel addSectionWithIdentifier:sectionID];

  // Adds default "New Folder" item if needed.
  if (_allowsNewFolders) {
    TableViewBookmarksFolderItem* createFolderItem =
        [[TableViewBookmarksFolderItem alloc]
            initWithType:ItemTypeCreateNewFolder
                   style:BookmarksFolderStyleNewFolder];
    createFolderItem.accessibilityIdentifier =
        (sectionID == SectionIdentifierLocalOrSyncableBookmarks)
            ? kBookmarkCreateNewLocalOrSyncableFolderCellIdentifier
            : kBookmarkCreateNewAccountFolderCellIdentifier;
    createFolderItem.shouldDisplayCloudSlashIcon =
        (sectionID == SectionIdentifierLocalOrSyncableBookmarks) &&
        [self.dataSource shouldDisplayCloudIconForLocalOrSyncableBookmarks];
    // Add the "New Folder" Item to the same section as the rest of the folder
    // entries.
    [self.tableViewModel addItem:createFolderItem
         toSectionWithIdentifier:sectionID];
  }

  // Add Folders entries.
  const std::vector<const BookmarkNode*>& folders =
      (sectionID == SectionIdentifierAccountBookmarks)
          ? _accountFolderNodes
          : _localOrSyncableFolderNodes;
  for (const BookmarkNode* folderNode : folders) {
    TableViewBookmarksFolderItem* folderItem =
        [[TableViewBookmarksFolderItem alloc]
            initWithType:ItemTypeBookmarkFolder
                   style:BookmarksFolderStyleFolderEntry];
    folderItem.title = bookmark_utils_ios::TitleForBookmarkNode(folderNode);
    folderItem.currentFolder =
        [self.dataSource selectedFolderNode] == folderNode;
    folderItem.accessibilityIdentifier = folderItem.title;
    folderItem.shouldDisplayCloudSlashIcon =
        (sectionID == SectionIdentifierLocalOrSyncableBookmarks) &&
        [self.dataSource shouldDisplayCloudIconForLocalOrSyncableBookmarks];

    // Indentation level.
    NSInteger level = 0;
    while (folderNode && !folderNode->is_root()) {
      ++level;
      folderNode = folderNode->parent();
    }
    // The root node is not shown as a folder, so top level folders have a
    // level strictly positive.
    DCHECK(level > 0);
    folderItem.indentationLevel = level - 1;
    [self.tableViewModel addItem:folderItem toSectionWithIdentifier:sectionID];
  }
}

- (TableViewHeaderFooterItem*)headerForSectionWithIdentifier:
    (SectionIdentifier)sectionID {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];

  switch (sectionID) {
    case SectionIdentifierLocalOrSyncableBookmarks:
      header.text =
          l10n_util::GetNSString(IDS_IOS_BOOKMARKS_PROFILE_SECTION_TITLE);
      break;
    case SectionIdentifierAccountBookmarks:
      header.text =
          l10n_util::GetNSString(IDS_IOS_BOOKMARKS_ACCOUNT_SECTION_TITLE);
      break;
  }
  return header;
}

- (void)delayedNotifyDelegateOfSelection {
  self.view.userInteractionEnabled = NO;
  __weak BookmarksFolderChooserViewController* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        BookmarksFolderChooserViewController* strongSelf = weakSelf;
        // Early return if the controller has been deallocated.
        if (!strongSelf) {
          return;
        }
        strongSelf.view.userInteractionEnabled = YES;
        [strongSelf done:nil];
      });
}

@end
