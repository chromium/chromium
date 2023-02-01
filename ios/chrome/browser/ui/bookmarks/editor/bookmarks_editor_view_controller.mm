// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_view_controller.h"

#import <memory>
#import <set>

#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/ios/block_types.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_cftyperef.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mutator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_view_controller.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

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

@interface BookmarksEditorViewController () <
    BookmarksFolderChooserViewControllerDelegate,
    BookmarkTextFieldItemDelegate>

// The folder picker view controller.
// Redefined to be readwrite.
@property(nonatomic, strong)
    BookmarksFolderChooserViewController* folderViewController;

@property(nonatomic, assign) Browser* browser;

@property(nonatomic, assign) ChromeBrowserState* browserState;

// Done button item in navigation bar.
@property(nonatomic, strong) UIBarButtonItem* doneItem;

// CollectionViewItem-s from the collection.
@property(nonatomic, strong) BookmarkTextFieldItem* nameItem;
@property(nonatomic, strong) BookmarkParentFolderItem* folderItem;
@property(nonatomic, strong) BookmarkTextFieldItem* URLItem;

// YES if the URL item is displaying a valid URL.
@property(nonatomic, assign) BOOL displayingValidURL;

// Reports the changes to the delegate, that has the responsibility to save the
// bookmark.
- (void)commitBookmarkChanges;

// The Save button is disabled if the form values are deemed non-valid. This
// method updates the state of the Save button accordingly.
- (void)updateSaveButtonState;

// Called when the Delete button is pressed.
- (void)deleteBookmark;

// Called when the Folder button is pressed.
- (void)moveBookmark;

@end

#pragma mark

@implementation BookmarksEditorViewController

@synthesize delegate = _delegate;
@synthesize displayingValidURL = _displayingValidURL;
@synthesize folderViewController = _folderViewController;
@synthesize browser = _browser;
@synthesize browserState = _browserState;
@synthesize cancelItem = _cancelItem;
@synthesize doneItem = _doneItem;
@synthesize nameItem = _nameItem;
@synthesize folderItem = _folderItem;
@synthesize URLItem = _URLItem;

#pragma mark - Lifecycle

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    // Browser may be OTR, which is why the original browser state is being
    // explicitly requested.
    _browser = browser;
    _browserState = browser->GetBrowserState()->GetOriginalChromeBrowserState();
  }
  return self;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
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
  _cancelItem = cancelItem;

  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
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
  base::AutoReset<BOOL> autoReset(
      [self.mutator ignoresBookmarkModelChangesPointer], YES);

  GURL url = ConvertUserDataToGURL([self inputURLString]);
  // If the URL was not valid, the `save` message shouldn't have been sent.
  DCHECK([self inputURLIsValid]);

  // Tell delegate if bookmark name or title has been changed.
  if ([self.mutator bookmark] &&
      ([self.mutator bookmark]->GetTitle() !=
           base::SysNSStringToUTF16([self inputBookmarkName]) ||
       [self.mutator bookmark]->url() != url)) {
    [self.delegate bookmarkEditorWillCommitTitleOrURLChange:self];
  }

  [self.snackbarCommandsHandler
      showSnackbarMessage:
          bookmark_utils_ios::CreateOrUpdateBookmarkWithUndoToast(
              [self.mutator bookmark], [self inputBookmarkName], url,
              [self.mutator folder], [self.mutator bookmarkModel],
              self.browserState)];
}

- (void)dismissBookmarkEditView {
  [self.view endEditing:YES];

  // Dismiss this controller.
  [self.delegate bookmarkEditorWantsDismissal:self];
}

// Enable or disable the left and right bar buttons.
- (void)sidesBarButton:(BOOL)enabled {
  self.navigationItem.leftBarButtonItem.enabled = enabled;
  self.navigationItem.rightBarButtonItem.enabled = enabled;
}

#pragma mark - Layout

- (void)setNavigationItemsEnabled:(BOOL)enabled {
  self.navigationItem.leftBarButtonItem.enabled = enabled;
  self.navigationItem.rightBarButtonItem.enabled = enabled;
}

- (void)updateSaveButtonState {
  self.doneItem.enabled = [self inputURLIsValid];
}

#pragma mark - BookmarksEditorConsumer

- (void)bookmarkDidMoveToParent:(const bookmarks::BookmarkNode*)newParent {
  [self.folderViewController changeSelectedFolder:newParent];
}

- (void)updateFolderLabel {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeFolder
                              sectionIdentifier:SectionIdentifierInfo];
  if (!indexPath) {
    return;
  }

  NSString* folderName = @"";
  if ([self.mutator bookmark]) {
    folderName =
        bookmark_utils_ios::TitleForBookmarkNode([self.mutator folder]);
  }

  self.folderItem.title = folderName;
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

- (void)updateUIFromBookmark {
  // If there is no current bookmark, don't update.
  if (![self.mutator bookmark]) {
    return;
  }

  [self loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierInfo];

  self.nameItem = [[BookmarkTextFieldItem alloc] initWithType:ItemTypeName];
  self.nameItem.accessibilityIdentifier = @"Title Field";
  self.nameItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NAME_FIELD_HEADER);
  self.nameItem.text =
      bookmark_utils_ios::TitleForBookmarkNode([self.mutator bookmark]);
  self.nameItem.delegate = self;
  [model addItem:self.nameItem toSectionWithIdentifier:SectionIdentifierInfo];

  self.folderItem =
      [[BookmarkParentFolderItem alloc] initWithType:ItemTypeFolder];
  self.folderItem.title =
      bookmark_utils_ios::TitleForBookmarkNode([self.mutator folder]);
  [model addItem:self.folderItem toSectionWithIdentifier:SectionIdentifierInfo];

  self.URLItem = [[BookmarkTextFieldItem alloc] initWithType:ItemTypeURL];
  self.URLItem.accessibilityIdentifier = @"URL Field";
  self.URLItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_URL_FIELD_HEADER);
  self.URLItem.text =
      base::SysUTF8ToNSString([self.mutator bookmark]->url().spec());
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
  if ([self.mutator bookmark] && [self.mutator bookmarkModel]->loaded()) {
    // To stop getting recursive events from committed bookmark editing changes
    // ignore bookmark model updates notifications.
    base::AutoReset<BOOL> autoReset(
        [self.mutator ignoresBookmarkModelChangesPointer], YES);

    // When launched from the star button, removing the current bookmark
    // removes all matching nodes.
    std::vector<const BookmarkNode*> nodesVector;
    [self.mutator bookmarkModel]->GetNodesByURL([self.mutator bookmark]->url(),
                                                &nodesVector);
    std::set<const BookmarkNode*> nodes(nodesVector.begin(), nodesVector.end());

    [self.snackbarCommandsHandler
        showSnackbarMessage:bookmark_utils_ios::DeleteBookmarksWithUndoToast(
                                nodes, [self.mutator bookmarkModel],
                                self.browserState)];
    [self.mutator setBookmark:nil];
  }
  [self.delegate bookmarkEditorWantsDismissal:self];
}

- (void)moveBookmark {
  DCHECK([self.mutator bookmarkModel]);
  DCHECK(!self.folderViewController);

  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert([self.mutator bookmark]);
  BookmarksFolderChooserViewController* folderViewController =
      [[BookmarksFolderChooserViewController alloc]
          initWithBookmarkModel:[self.mutator bookmarkModel]
               allowsNewFolders:YES
                    editedNodes:editedNodes
                   allowsCancel:NO
                 selectedFolder:[self.mutator folder]
                        browser:_browser];
  folderViewController.delegate = self;
  folderViewController.snackbarCommandsHandler = self.snackbarCommandsHandler;
  self.folderViewController = folderViewController;
  self.folderViewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  [self.navigationController pushViewController:self.folderViewController
                                       animated:YES];
}

- (void)cancel {
  [self dismissBookmarkEditView];
}

- (void)save {
  [self commitBookmarkChanges];
  [self dismissBookmarkEditView];
}

#pragma mark - BookmarkTextFieldItemDelegate

- (void)textDidChangeForItem:(BookmarkTextFieldItem*)item {
  self.modalInPresentation = YES;
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
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
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
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] == ItemTypeFolder) {
    [self moveBookmark];
  }
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
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

#pragma mark - BookmarksFolderChooserViewControllerDelegate

- (void)folderPicker:(BookmarksFolderChooserViewController*)folderPicker
    didFinishWithFolder:(const BookmarkNode*)folder {
  [self.mutator changeFolder:folder];
  // This delegate method can be called on two occasions:
  // - the user selected a folder in the folder picker. In that case, the folder
  // picker should be popped;
  // - the user created a new folder, in which case the navigation stack
  // contains this bookmark editor (`self`), a folder picker and a folder
  // creator. In such a case, both the folder picker and creator shoud be popped
  // to reveal this bookmark editor. Thus the call to
  // `popToViewController:animated:`.
  [self.navigationController popToViewController:self animated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidCancel:
    (BookmarksFolderChooserViewController*)folderPicker {
  // This delegate method can only be called from the folder picker, which is
  // the only view controller on top of this bookmark editor (`self`). Thus the
  // call to `popViewControllerAnimated:`.
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidDismiss:
    (BookmarksFolderChooserViewController*)folderPicker {
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
  [self dismissBookmarkEditView];
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self dismissBookmarkEditView];
}

@end
