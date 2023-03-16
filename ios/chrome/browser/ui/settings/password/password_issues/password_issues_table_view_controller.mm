// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_table_view_controller.h"

#import <UIKit/UIKit.h>
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue_content_item.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_presenter.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypePassword,  // This is a repeated item type.
};

}  // namespace

@interface PasswordIssuesTableViewController () {
  // Text of the header displayed on top of the page.
  NSString* _headerText;
  // URL of link in the page header. Nullable.
  CrURL* _headerURL;
}

@property(nonatomic, strong) NSArray<PasswordIssue*>* passwords;

@end

@implementation PasswordIssuesTableViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordIssuesTableViewId;

  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presenter dismissPasswordIssuesTableViewController];
  }
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  TableViewLinkHeaderFooterItem* headerItem = [self headerItem];

  if (headerItem) {
    // Set header on top of first section.
    [model setHeader:headerItem
        forSectionWithIdentifier:kSectionIdentifierEnumZero];
  }

  for (PasswordIssue* password in self.passwords) {
    [model addItem:[self passwordIssueItem:password]
        toSectionWithIdentifier:SectionIdentifierContent];
  }
}

#pragma mark - Items

- (TableViewLinkHeaderFooterItem*)headerItem {
  if (!_headerText) {
    return nil;
  }

  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.text = _headerText;

  if (_headerURL) {
    headerItem.urls = @[ _headerURL ];
  }

  return headerItem;
}

- (PasswordIssueContentItem*)passwordIssueItem:(PasswordIssue*)password {
  PasswordIssueContentItem* passwordItem =
      [[PasswordIssueContentItem alloc] initWithType:ItemTypePassword];
  passwordItem.password = password;
  passwordItem.accessibilityTraits |= UIAccessibilityTraitButton;
  passwordItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  return passwordItem;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeHeader:
      break;
    case ItemTypePassword: {
      PasswordIssueContentItem* passwordIssue =
          base::mac::ObjCCastStrict<PasswordIssueContentItem>(
              [model itemAtIndexPath:indexPath]);
      [self.presenter presentPasswordIssueDetails:passwordIssue.password];
      break;
    }
  }

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypePassword: {
      TableViewURLCell* urlCell =
          base::mac::ObjCCastStrict<TableViewURLCell>(cell);
      urlCell.textLabel.lineBreakMode = NSLineBreakByTruncatingHead;
      // Load the favicon from cache.
      [self loadFaviconAtIndexPath:indexPath forCell:cell];
      break;
    }
  }
  return cell;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForHeaderInSection:section];

  if (section == 0 && [self.tableViewModel headerForSectionIndex:0]) {
    // Attach self as delegate to handle clicks in page header.
    TableViewLinkHeaderFooterView* headerView =
        base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    headerView.delegate = self;
  }

  return view;
}

// Asynchronously loads favicon for given index path. The loads are cancelled
// upon cell reuse automatically.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
                       forCell:(UITableViewCell*)cell {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  DCHECK(cell);

  TableViewURLItem* URLItem = base::mac::ObjCCastStrict<TableViewURLItem>(item);
  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);

  NSString* itemIdentifier = URLItem.uniqueIdentifier;
  [self.imageDataSource
      faviconForPageURL:URLItem.URL
             completion:^(FaviconAttributes* attributes) {
               // Only set favicon if the cell hasn't been reused.
               if ([URLCell.cellUniqueIdentifier
                       isEqualToString:itemIdentifier]) {
                 DCHECK(attributes);
                 [URLCell.faviconView configureWithAttributes:attributes];
               }
             }];
}

#pragma mark - PasswordIssuesConsumer

- (void)setPasswordIssues:(NSArray<PasswordIssueGroup*>*)passwordGroups {
  // TODO(crbug.com/1406540): Replace passwords with passwordGroups to display
  // all groups.
  self.passwords =
      passwordGroups.count == 0 ? @[] : passwordGroups[0].passwordIssues;
  [self reloadData];
}

- (void)setNavigationBarTitle:(NSString*)title {
  self.title = title;
}

- (void)setHeader:(NSString*)text URL:(CrURL*)URL {
  _headerText = text;
  _headerURL = URL;

  [self reloadData];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.presenter dismissAndOpenURL:URL];
}

@end
