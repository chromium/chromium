// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/constants.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_table_view_item.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_view_controller_delegate.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  TabListSectionIdentifier = kSectionIdentifierEnumZero,
};

}  // namespace

@implementation TabListFromAndroidViewController

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.definesPresentationContext = YES;
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  self.title =
      l10n_util::GetNSString(IDS_IOS_BRING_ANDROID_TABS_REVIEW_TABLE_TITLE);
  [self.tableView setDelegate:self];
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.allowsMultipleSelection = YES;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewSeparatorInset, 0, 0)];
  self.tableView.estimatedRowHeight = kTabListFromAndroidCellHeight;
  self.navigationItem.leftBarButtonItem = [self navigationCancelButton];
  self.navigationItem.rightBarButtonItem = [self navigationOpenTabsButton];
  self.tableView.accessibilityIdentifier = kBringAndroidTabsPromptTabListAXId;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  CHECK_EQ(tableView, self.tableView);
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  item.accessoryType = UITableViewCellAccessoryCheckmark;
  [self reconfigureCellsForItems:@[ item ]];
  [self updateOpenTabsButton];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  CHECK_EQ(tableView, self.tableView);
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  item.accessoryType = UITableViewCellAccessoryNone;
  [self reconfigureCellsForItems:@[ item ]];
  [self updateOpenTabsButton];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  CHECK_EQ(tableView, self.tableView);
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  [self loadFaviconForCell:cell indexPath:indexPath];
  return cell;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  int numberDeselected =
      [self.tableView
          numberOfRowsInSection:
              [self.tableViewModel
                  sectionForSectionIdentifier:kSectionIdentifierEnumZero]] -
      [self.tableView indexPathsForSelectedRows].count;
  [_delegate
      tabListFromAndroidViewControllerDidDismissWithSwipe:YES
                                   numberOfDeselectedTabs:numberDeselected];
}

#pragma mark - TabListFromAndroidConsumer

- (void)setTabListItems:(NSArray<TabListFromAndroidTableViewItem*>*)items {
  [self loadModel];

  [self.tableViewModel addSectionWithIdentifier:TabListSectionIdentifier];
  for (TabListFromAndroidTableViewItem* item : items) {
    item.accessoryType = UITableViewCellAccessoryCheckmark;
    [self.tableViewModel addItem:item
         toSectionWithIdentifier:TabListSectionIdentifier];
  }
  // Tabs are selected by default.
  NSInteger section = [self.tableViewModel
      sectionForSectionIdentifier:TabListSectionIdentifier];
  for (NSInteger index = 0;
       index < [self.tableViewModel numberOfItemsInSection:section]; index++) {
    [self.tableView selectRowAtIndexPath:[NSIndexPath indexPathForRow:index
                                                            inSection:section]
                                animated:NO
                          scrollPosition:UITableViewScrollPositionNone];
  }

  [self updateOpenTabsButton];
}

#pragma mark - Helpers

// Retrieves favicon from FaviconLoader and sets FaviconView in given `cell`.
- (void)loadFaviconForCell:(UITableViewCell*)cell
                 indexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  CHECK(item);
  CHECK(cell);
  TabListFromAndroidTableViewItem* tabListItem =
      base::apple::ObjCCastStrict<TabListFromAndroidTableViewItem>(item);
  TabListFromAndroidTableViewCell* tabListCell =
      base::apple::ObjCCastStrict<TabListFromAndroidTableViewCell>(cell);

  NSString* itemIdentifier = tabListItem.uniqueIdentifier;
  [_faviconDataSource
      faviconForPageURL:tabListItem.URL
             completion:^(FaviconAttributes* attributes) {
               // Only set favicon if the cell hasn't been reused.
               if ([tabListCell.cellUniqueIdentifier
                       isEqualToString:itemIdentifier]) {
                 [tabListCell.faviconView configureWithAttributes:attributes];
               }
             }];
}

// Returns the navigation 'cancel' button.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  cancelButton.accessibilityIdentifier =
      kBringAndroidTabsPromptTabListCancelButtonAXId;
  return cancelButton;
}

// Updates the title and enabled status of the 'Open Tabs' button. This method
// is called and the button is recreated every time the user selects/de-selects
// a table row.
- (void)updateOpenTabsButton {
  UIBarButtonItem* rightBarButton = self.navigationItem.rightBarButtonItem;
  rightBarButton.title = [self openTabsButtonTitle];
  int numberSelectedTabs = [self.tableView indexPathsForSelectedRows].count;
  if (numberSelectedTabs == 0) {
    [rightBarButton setEnabled:NO];
  } else {
    [rightBarButton setEnabled:YES];
  }
}

// Returns the title of the navigation 'Open Tabs' button.
- (NSString*)openTabsButtonTitle {
  int numberSelectedTabs = [self.tableView indexPathsForSelectedRows].count;
  return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
      IDS_IOS_BRING_ANDROID_TABS_REVIEW_TABLE_OPEN_BUTTON, numberSelectedTabs));
}

// Returns the navigation 'Open Tabs' button.
- (UIBarButtonItem*)navigationOpenTabsButton {
  UIBarButtonItem* openButton =
      [[UIBarButtonItem alloc] initWithTitle:[self openTabsButtonTitle]
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(openButtonTapped)];
  openButton.accessibilityIdentifier =
      kBringAndroidTabsPromptTabListOpenButtonAXId;
  return openButton;
}

// Called when the cancel button is tapped.
- (void)cancelButtonTapped {
  int numberDeselected =
      [self.tableView
          numberOfRowsInSection:
              [self.tableViewModel
                  sectionForSectionIdentifier:kSectionIdentifierEnumZero]] -
      [self.tableView indexPathsForSelectedRows].count;
  [_delegate
      tabListFromAndroidViewControllerDidDismissWithSwipe:NO
                                   numberOfDeselectedTabs:numberDeselected];
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

//  Called when the open tabs button is tapped.
- (void)openButtonTapped {
  [_delegate tabListFromAndroidViewControllerDidTapOpenButtonWithTabIndices:
                 [self selectedTabIndices]];
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

// Returns an array with the indices of the tabs the user has selected. Indices
// correspond to the indices of the list of user's tabs retrieved from
// BringAndroidTabsToIOSService.
- (NSArray<NSNumber*>*)selectedTabIndices {
  NSMutableArray<NSNumber*>* tabIndices = [[NSMutableArray alloc] init];
  for (NSIndexPath* indexPath in self.tableView.indexPathsForSelectedRows) {
    [tabIndices addObject:@(indexPath.row)];
  }
  return tabIndices;
}

@end
