// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_view_controller.h"

#import <memory>
#import <set>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_cftyperef.h"
#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_mediator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_text_field_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_mutator.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

using bookmarks::BookmarkNode;

namespace {

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

@interface BookmarksEditorViewController () <BookmarkTextFieldItemDelegate> {
  // The name of the presented bookmark.
  NSString* _name;
  // The URL of the presented bookmark.
  NSString* _URL;
  // The name of the folder of the presented bookmark.
  NSString* _folderName;
}

// Done button item in navigation bar.
@property(nonatomic, strong) UIBarButtonItem* doneItem;

// CollectionViewItem-s from the collection.
@property(nonatomic, strong) BookmarkTextFieldItem* nameItem;
@property(nonatomic, strong) BookmarkParentFolderItem* folderItem;
@property(nonatomic, strong) BookmarkTextFieldItem* URLItem;

// YES if the URL item is displaying a valid URL.
@property(nonatomic, assign) BOOL displayingValidURL;

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
@synthesize cancelItem = _cancelItem;
@synthesize doneItem = _doneItem;
@synthesize nameItem = _nameItem;
@synthesize folderItem = _folderItem;
@synthesize URLItem = _URLItem;

#pragma mark - Lifecycle

- (instancetype)initWithName:(NSString*)name
                         URL:(NSString*)URL
                  folderName:(NSString*)folderName {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _name = name;
    _URL = URL;
    _folderName = folderName;
    _canBeDismissed = YES;
  }
  return self;
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

  [self updateUI];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be shown.
  self.navigationController.toolbarHidden = NO;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self cancel];
  return YES;
}

#pragma mark - Private

- (void)updateUI {
  [self loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierInfo];

  self.nameItem = [[BookmarkTextFieldItem alloc] initWithType:ItemTypeName];
  self.nameItem.accessibilityIdentifier = @"Title Field";
  self.nameItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NAME_FIELD_HEADER);
  self.nameItem.text = _name;
  self.nameItem.delegate = self;
  [model addItem:self.nameItem toSectionWithIdentifier:SectionIdentifierInfo];

  self.folderItem =
      [[BookmarkParentFolderItem alloc] initWithType:ItemTypeFolder];
  self.folderItem.title = _folderName;
  self.folderItem.shouldDisplayCloudSlashIcon =
      [self.mutator shouldDisplayCloudSlashSymbolForParentFolder];
  [model addItem:self.folderItem toSectionWithIdentifier:SectionIdentifierInfo];

  self.URLItem = [[BookmarkTextFieldItem alloc] initWithType:ItemTypeURL];
  self.URLItem.accessibilityIdentifier = @"URL Field";
  self.URLItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_URL_FIELD_HEADER);
  self.URLItem.text = _URL;
  self.URLItem.delegate = self;
  [model addItem:self.URLItem toSectionWithIdentifier:SectionIdentifierInfo];

  TableViewHeaderFooterItem* errorFooter =
      [[TableViewHeaderFooterItem alloc] initWithType:ItemTypeInvalidURLFooter];
  [model setFooter:errorFooter forSectionWithIdentifier:SectionIdentifierInfo];
  self.displayingValidURL = YES;

  // Save button state.
  [self updateSaveButtonState];
}

- (BOOL)inputURLIsValid {
  return bookmark_utils_ios::ConvertUserDataToGURL([self inputURLString])
      .is_valid();
}

// Retrieves input URL string from UI.
- (NSString*)inputURLString {
  return self.URLItem.text;
}

// Retrieves input bookmark name string from UI.
- (NSString*)inputBookmarkName {
  return self.nameItem.text;
}

- (void)dismissBookmarkEditorView {
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

- (void)updateFolderLabel:(NSString*)folderName {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeFolder
                              sectionIdentifier:SectionIdentifierInfo];
  if (!indexPath) {
    return;
  }
  _folderName = folderName;
  self.folderItem.title = folderName;
  self.folderItem.shouldDisplayCloudSlashIcon =
      [self.mutator shouldDisplayCloudSlashSymbolForParentFolder];
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

- (void)updateUIWithName:(NSString*)name
                     URL:(NSString*)URL
              folderName:(NSString*)folderName {
  _name = name;
  _URL = URL;
  _folderName = folderName;
  [self updateUI];
}

- (void)updateSync {
  if (!self.tableViewModel) {
    // If the view is not yet loaded, tableViewModel is nil, and we should no
    // update. Sync can occur before the view is loaded. See crbug.com/1429435
    return;
  }
  self.folderItem.shouldDisplayCloudSlashIcon =
      [self.mutator shouldDisplayCloudSlashSymbolForParentFolder];
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeFolder
                              sectionIdentifier:SectionIdentifierInfo];
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

#pragma mark - Actions

- (void)deleteBookmark {
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksEditorDeletedBookmark"));
  [self.mutator deleteBookmark];
}

- (void)moveBookmark {
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksEditorOpenedFolderChooser"));
  [self.delegate moveBookmark];
}

- (void)cancel {
  base::RecordAction(base::UserMetricsAction("MobileBookmarksEditorCanceled"));
  [self dismissBookmarkEditorView];
}

- (void)save {
  base::RecordAction(base::UserMetricsAction("MobileBookmarksEditorSaved"));
  [self.mutator commitBookmarkChangesWithURLString:[self inputURLString]
                                              name:[self inputBookmarkName]];
  [self dismissBookmarkEditorView];
}

#pragma mark - BookmarkTextFieldItemDelegate

- (void)textDidChangeForItem:(BookmarkTextFieldItem*)item {
  self.edited = YES;
  [self updateSaveButtonState];
  if (self.displayingValidURL != [self inputURLIsValid]) {
    self.displayingValidURL = [self inputURLIsValid];
    self.canBeDismissed = self.displayingValidURL;
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
        base::apple::ObjCCastStrict<UITableViewHeaderFooterView>(footerView);
    headerFooterView.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    headerFooterView.textLabel.textColor = [UIColor colorNamed:kRedColor];
  }
  return footerView;
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
  [self dismissBookmarkEditorView];
}

@end
