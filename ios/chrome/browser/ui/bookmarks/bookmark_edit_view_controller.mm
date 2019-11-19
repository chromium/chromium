// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"

#include <memory>
#include <set>

#include "base/auto_reset.h"
#include "base/ios/block_types.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/url_formatter/url_fixer.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {
// Converts NSString entered by the user to a GURL.
GURL ConvertUserDataToGURL(NSString* urlString) {
  if (urlString) {
    return url_formatter::FixupURL(base::SysNSStringToUTF8(urlString),
                                   std::string());
  } else {
    return GURL();
  }
}

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInfo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeName = kItemTypeEnumZero,
  ItemTypeFolder,
  ItemTypeURL,
  ItemTypeInvalidURLFooter,
};

// Estimated Table Row height.
const CGFloat kEstimatedTableRowHeight = 50;
// Estimated TableSection Footer height.
const CGFloat kEstimatedTableSectionFooterHeight = 40;
}  // namespace

@interface BookmarkEditViewController () <BookmarkFolderViewControllerDelegate,
                                          BookmarkModelBridgeObserver,
                                          BookmarkTextFieldItemDelegate> {
  // Flag to ignore bookmark model changes notifications.
  BOOL _ignoresBookmarkModelChanges;

  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
}

// The bookmark this controller displays or edits.
// Redefined to be readwrite.
@property(nonatomic, assign) const BookmarkNode* bookmark;

// Reference to the bookmark model.
@property(nonatomic, assign) BookmarkModel* bookmarkModel;

// The parent of the bookmark. This may be different from |bookmark->parent()|
// if the changes have not been saved yet. |folder| then represents the
// candidate for the new parent of |bookmark|.  This property is always a
// non-NULL, valid folder.
@property(nonatomic, assign) const BookmarkNode* folder;

// The folder picker view controller.
// Redefined to be readwrite.
@property(nonatomic, strong) BookmarkFolderViewController* folderViewController;

@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// Dispatcher for sending commands.
@property(nonatomic, readonly, weak) id<BrowserCommands> dispatcher;

// Cancel button item in navigation bar.
@property(nonatomic, strong) UIBarButtonItem* cancelItem;

// Done button item in navigation bar.
@property(nonatomic, strong) UIBarButtonItem* doneItem;

// CollectionViewItem-s from the collection.
@property(nonatomic, strong) BookmarkTextFieldItem* nameItem;
@property(nonatomic, strong) BookmarkParentFolderItem* folderItem;
@property(nonatomic, strong) BookmarkTextFieldItem* URLItem;

// YES if the URL item is displaying a valid URL.
@property(nonatomic, assign) BOOL displayingValidURL;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Reports the changes to the delegate, that has the responsibility to save the
// bookmark.
- (void)commitBookmarkChanges;

// Changes |self.folder| and updates the UI accordingly.
// The change is not committed until the user taps the Save button.
- (void)changeFolder:(const BookmarkNode*)folder;

// The Save button is disabled if the form values are deemed non-valid. This
// method updates the state of the Save button accordingly.
- (void)updateSaveButtonState;

// Reloads the folder label text.
- (void)updateFolderLabel;

// Populates the UI with information from the models.
- (void)updateUIFromBookmark;

// Called when the Delete button is pressed.
- (void)deleteBookmark;

// Called when the Folder button is pressed.
- (void)moveBookmark;

// Called when the Cancel button is pressed.
- (void)cancel;

// Called when the Done button is pressed.
- (void)save;

@end

#pragma mark

@implementation BookmarkEditViewController

@synthesize bookmark = _bookmark;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize delegate = _delegate;
@synthesize displayingValidURL = _displayingValidURL;
@synthesize folder = _folder;
@synthesize folderViewController = _folderViewController;
@synthesize browserState = _browserState;
@synthesize cancelItem = _cancelItem;
@synthesize doneItem = _doneItem;
@synthesize nameItem = _nameItem;
@synthesize folderItem = _folderItem;
@synthesize URLItem = _URLItem;

#pragma mark - Lifecycle

- (instancetype)initWithBookmark:(const BookmarkNode*)bookmark
                    browserState:(ios::ChromeBrowserState*)browserState
                      dispatcher:(id<BrowserCommands>)dispatcher {
  DCHECK(bookmark);
  DCHECK(browserState);
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    DCHECK(!bookmark->is_folder());
    DCHECK(!browserState->IsOffTheRecord());
    _bookmark = bookmark;
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(browserState);

    _folder = bookmark->parent();

    // Set up the bookmark model oberver.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    _browserState = browserState;
    _dispatcher = dispatcher;
  }
  return self;
}

- (void)dealloc {
  _folderViewController.delegate = nil;
}

#pragma mark View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor = self.styler.tableViewBackgroundColor;
  self.tableView.estimatedRowHeight = kEstimatedTableRowHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionFooterHeight =
      kEstimatedTableSectionFooterHeight;
  self.view.accessibilityIdentifier = kBookmarkEditViewContainerIdentifier;

  // Add a tableFooterView in order to disable separators at the bottom of the
  // tableView.
  self.tableView.tableFooterView = [[UIView alloc] init];
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kBookmarkCellHorizontalLeadingInset,
                                         0, 0)];

  self.title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_EDIT_SCREEN_TITLE);

  self.navigationItem.hidesBackButton = YES;

  UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancel)];
  cancelItem.accessibilityIdentifier = @"Cancel";
  self.navigationItem.leftBarButtonItem = cancelItem;
  self.cancelItem = cancelItem;

  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_DONE_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(save)];
  doneItem.accessibilityIdentifier =
      kBookmarkEditNavigationBarDoneButtonIdentifier;
  self.navigationItem.rightBarButtonItem = doneItem;
  self.doneItem = doneItem;

  // Setup the bottom toolbar.
  NSString* titleString = l10n_util::GetNSString(IDS_IOS_BOOKMARK_DELETE);
  UIBarButtonItem* deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(deleteBookmark)];
  deleteButton.accessibilityIdentifier = kBookmarkEditDeleteButtonIdentifier;
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  deleteButton.tintColor = [UIColor colorNamed:kRedColor];
  // Setting the image to nil will cause the default shadowImage to be used,
  // we need to create a new one.
  [self.navigationController.toolbar setShadowImage:[UIImage new]
                                 forToolbarPosition:UIBarPositionAny];
  [self setToolbarItems:@[ spaceButton, deleteButton, spaceButton ]
               animated:NO];

  [self updateUIFromBookmark];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be shown.
  self.navigationController.toolbarHidden = NO;
}

#pragma mark - Presentation controller integration

- (BOOL)shouldBeDismissedOnTouchOutside {
  return NO;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self cancel];
  return YES;
}

#pragma mark - Private

- (BOOL)inputURLIsValid {
  return ConvertUserDataToGURL([self inputURLString]).is_valid();
}

// Retrieves input URL string from UI.
- (NSString*)inputURLString {
  return self.URLItem.text;
}

// Retrieves input bookmark name string from UI.
- (NSString*)inputBookmarkName {
  return self.nameItem.text;
}

- (void)commitBookmarkChanges {
  // To stop getting recursive events from committed bookmark editing changes
  // ignore bookmark model updates notifications.
  base::AutoReset<BOOL> autoReset(&_ignoresBookmarkModelChanges, YES);

  GURL url = ConvertUserDataToGURL([self inputURLString]);
  // If the URL was not valid, the |save| message shouldn't have been sent.
  DCHECK([self inputURLIsValid]);

  // Tell delegate if bookmark name or title has been changed.
  if (self.bookmark &&
      (self.bookmark->GetTitle() !=
           base::SysNSStringToUTF16([self inputBookmarkName]) ||
       self.bookmark->url() != url)) {
    [self.delegate bookmarkEditorWillCommitTitleOrUrlChange:self];
  }

  [self.dispatcher showSnackbarMessage:
                       bookmark_utils_ios::CreateOrUpdateBookmarkWithUndoToast(
                           self.bookmark, [self inputBookmarkName], url,
                           self.folder, self.bookmarkModel, self.browserState)];
}

- (void)changeFolder:(const BookmarkNode*)folder {
  DCHECK(folder->is_folder());
  self.folder = folder;
  [BookmarkMediator setFolderForNewBookmarks:self.folder
                              inBrowserState:self.browserState];
  [self updateFolderLabel];
}

- (void)dismiss {
  [self.view endEditing:YES];

  // Dismiss this controller.
  [self.delegate bookmarkEditorWantsDismissal:self];
}

#pragma mark - Layout

- (void)updateSaveButtonState {
  self.doneItem.enabled = [self inputURLIsValid];
}

- (void)updateFolderLabel {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeFolder
                              sectionIdentifier:SectionIdentifierInfo];
  NSString* folderName = @"";
  if (self.bookmark) {
    folderName = bookmark_utils_ios::TitleForBookmarkNode(self.folder);
  }

  self.folderItem.title = folderName;
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

- (void)updateUIFromBookmark {
  // If there is no current bookmark, don't update.
  if (!self.bookmark)
    return;

  [self loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierInfo];

  self.nameItem = [[BookmarkTextFieldItem alloc] initWithType:ItemTypeName];
  self.nameItem.accessibilityIdentifier = @"Title Field";
  self.nameItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NAME_FIELD_HEADER);
  self.nameItem.text = bookmark_utils_ios::TitleForBookmarkNode(self.bookmark);
  self.nameItem.delegate = self;
  [model addItem:self.nameItem toSectionWithIdentifier:SectionIdentifierInfo];

  self.folderItem =
      [[BookmarkParentFolderItem alloc] initWithType:ItemTypeFolder];
  self.folderItem.title = bookmark_utils_ios::TitleForBookmarkNode(self.folder);
  [model addItem:self.folderItem toSectionWithIdentifier:SectionIdentifierInfo];

  self.URLItem = [[BookmarkTextFieldItem alloc] initWithType:ItemTypeURL];
  self.URLItem.accessibilityIdentifier = @"URL Field";
  self.URLItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_URL_FIELD_HEADER);
  self.URLItem.text = base::SysUTF8ToNSString(self.bookmark->url().spec());
  self.URLItem.delegate = self;
  [model addItem:self.URLItem toSectionWithIdentifier:SectionIdentifierInfo];

  TableViewHeaderFooterItem* errorFooter =
      [[TableViewHeaderFooterItem alloc] initWithType:ItemTypeInvalidURLFooter];
  [model setFooter:errorFooter forSectionWithIdentifier:SectionIdentifierInfo];
  self.displayingValidURL = YES;

  // Save button state.
  [self updateSaveButtonState];
}

#pragma mark - Actions

- (void)deleteBookmark {
  if (self.bookmark && self.bookmarkModel->loaded()) {
    // To stop getting recursive events from committed bookmark editing changes
    // ignore bookmark model updates notifications.
    base::AutoReset<BOOL> autoReset(&_ignoresBookmarkModelChanges, YES);

    std::set<const BookmarkNode*> nodes;
    if ([self.delegate bookmarkEditor:self
            shoudDeleteAllOccurencesOfBookmark:self.bookmark]) {
      // When launched from the star button, removing the current bookmark
      // removes all matching nodes.
      std::vector<const BookmarkNode*> nodesVector;
      self.bookmarkModel->GetNodesByURL(self.bookmark->url(), &nodesVector);
      for (const BookmarkNode* node : nodesVector)
        nodes.insert(node);
    } else {
      // When launched from the info button, removing the current bookmark only
      // removes the current node.
      nodes.insert(self.bookmark);
    }
    [self.dispatcher
        showSnackbarMessage:bookmark_utils_ios::DeleteBookmarksWithUndoToast(
                                nodes, self.bookmarkModel, self.browserState)];
    self.bookmark = nil;
  }
  [self.delegate bookmarkEditorWantsDismissal:self];
}

- (void)moveBookmark {
  DCHECK(self.bookmarkModel);
  DCHECK(!self.folderViewController);

  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(self.bookmark);
  BookmarkFolderViewController* folderViewController =
      [[BookmarkFolderViewController alloc]
          initWithBookmarkModel:self.bookmarkModel
               allowsNewFolders:YES
                    editedNodes:editedNodes
                   allowsCancel:NO
                 selectedFolder:self.folder
                     dispatcher:self.dispatcher];
  folderViewController.delegate = self;
  self.folderViewController = folderViewController;
  self.folderViewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  [self.navigationController pushViewController:self.folderViewController
                                       animated:YES];
}

- (void)cancel {
  [self dismiss];
}

- (void)save {
  [self commitBookmarkChanges];
  [self dismiss];
}

#pragma mark - BookmarkTextFieldItemDelegate

- (void)textDidChangeForItem:(BookmarkTextFieldItem*)item {
  if (@available(iOS 13, *)) {
    self.modalInPresentation = YES;
  }
  [self updateSaveButtonState];
  if (self.displayingValidURL != [self inputURLIsValid]) {
    self.displayingValidURL = [self inputURLIsValid];
    UITableViewHeaderFooterView* footer = [self.tableView
        footerViewForSection:[self.tableViewModel sectionForSectionIdentifier:
                                                      SectionIdentifierInfo]];
    NSString* footerText =
        [self inputURLIsValid]
            ? @""
            : l10n_util::GetNSString(
                  IDS_IOS_BOOKMARK_URL_FIELD_VALIDATION_FAILED);
    [self.tableView beginUpdates];
    footer.textLabel.text = footerText;
    [self.tableView endUpdates];
  }
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

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  NSInteger type = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeName:
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    case ItemTypeURL:
    case ItemTypeFolder:
      break;
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] == ItemTypeFolder)
    [self moveBookmark];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView =
      [super tableView:tableView viewForFooterInSection:section];
  if (section ==
      [self.tableViewModel sectionForSectionIdentifier:SectionIdentifierInfo]) {
    UITableViewHeaderFooterView* headerFooterView =
        base::mac::ObjCCastStrict<UITableViewHeaderFooterView>(footerView);
    headerFooterView.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    headerFooterView.textLabel.textColor = [UIColor colorNamed:kRedColor];
  }
  return footerView;
}

#pragma mark - BookmarkFolderViewControllerDelegate

- (void)folderPicker:(BookmarkFolderViewController*)folderPicker
    didFinishWithFolder:(const BookmarkNode*)folder {
  [self changeFolder:folder];
  // This delegate method can be called on two occasions:
  // - the user selected a folder in the folder picker. In that case, the folder
  // picker should be popped;
  // - the user created a new folder, in which case the navigation stack
  // contains this bookmark editor (|self|), a folder picker and a folder
  // creator. In such a case, both the folder picker and creator shoud be popped
  // to reveal this bookmark editor. Thus the call to
  // |popToViewController:animated:|.
  [self.navigationController popToViewController:self animated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidCancel:(BookmarkFolderViewController*)folderPicker {
  // This delegate method can only be called from the folder picker, which is
  // the only view controller on top of this bookmark editor (|self|). Thus the
  // call to |popViewControllerAnimated:|.
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidDismiss:(BookmarkFolderViewController*)folderPicker {
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
  [self dismiss];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // No-op.
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (_ignoresBookmarkModelChanges)
    return;

  if (self.bookmark == bookmarkNode)
    [self updateUIFromBookmark];
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  if (_ignoresBookmarkModelChanges)
    return;

  [self updateFolderLabel];
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (_ignoresBookmarkModelChanges)
    return;

  if (self.bookmark == bookmarkNode)
    [self.folderViewController changeSelectedFolder:newParent];
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode
                 fromFolder:(const BookmarkNode*)folder {
  if (_ignoresBookmarkModelChanges)
    return;

  if (self.bookmark == bookmarkNode) {
    self.bookmark = nil;
    [self.delegate bookmarkEditorWantsDismissal:self];
  } else if (self.folder == bookmarkNode) {
    [self changeFolder:self.bookmarkModel->mobile_node()];
  }
}

- (void)bookmarkModelRemovedAllNodes {
  if (_ignoresBookmarkModelChanges)
    return;

  self.bookmark = nil;
  if (!self.bookmarkModel->is_permanent_node(self.folder)) {
    [self changeFolder:self.bookmarkModel->mobile_node()];
  }

  [self.delegate bookmarkEditorWantsDismissal:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                           title:nil
                         message:nil
                   barButtonItem:self.cancelItem];

  __weak __typeof(self) weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES)
                action:^{
                  [weakSelf save];
                }
                 style:UIAlertActionStyleDefault];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf cancel];
                }
                 style:UIAlertActionStyleDestructive];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  weakSelf.navigationItem.leftBarButtonItem.enabled = YES;
                  weakSelf.navigationItem.rightBarButtonItem.enabled = YES;
                }
                 style:UIAlertActionStyleCancel];

  self.navigationItem.leftBarButtonItem.enabled = NO;
  self.navigationItem.rightBarButtonItem.enabled = NO;
  [self.actionSheetCoordinator start];
}

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  // Resign first responder if trying to dismiss the VC so the keyboard doesn't
  // linger until the VC dismissal has completed.
  [self.view endEditing:YES];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismiss];
}

#pragma mark - UIResponder

- (NSArray*)keyCommands {
  __weak BookmarkEditViewController* weakSelf = self;
  return @[ [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                                   modifierFlags:Cr_UIKeyModifierNone
                                           title:nil
                                          action:^{
                                            [weakSelf dismiss];
                                          }] ];
}

@end
