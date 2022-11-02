// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_table_view_controller.h"

#import <UIKit/UIKit.h>
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/password/password_issue_content_item.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_presenter.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"
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

@interface PasswordIssuesTableViewController ()

@property(nonatomic) NSArray<id<PasswordIssue>>* passwords;

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
  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORDS);

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  [model setHeader:[self compromisedPasswordsDescriptionItem]
      forSectionWithIdentifier:SectionIdentifierContent];

    for (PasswordIssue* password in self.passwords) {
      [model addItem:[self passwordIssueItem:password]
          toSectionWithIdentifier:SectionIdentifierContent];
    }
}

#pragma mark - Items

- (TableViewLinkHeaderFooterItem*)compromisedPasswordsDescriptionItem {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  footerItem.text = l10n_util::GetNSString(IDS_IOS_PASSWORD_ISSUES_DESCRIPTION);
  return footerItem;
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
      faviconForURL:URLItem.URL
         completion:^(FaviconAttributes* attributes) {
           // Only set favicon if the cell hasn't been reused.
           if ([URLCell.cellUniqueIdentifier isEqualToString:itemIdentifier]) {
             DCHECK(attributes);
             [URLCell.faviconView configureWithAttributes:attributes];
           }
         }];
}

#pragma mark - PasswordIssuesConsumer

- (void)setPasswordIssues:(NSArray<id<PasswordIssue>>*)passwords {
  self.passwords = passwords;
  [self reloadData];
}

@end
