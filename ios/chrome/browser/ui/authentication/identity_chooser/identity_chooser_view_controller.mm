// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_add_account_item.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_header_item.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_view_controller_presentation_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

const CGFloat kViewControllerWidth = 312.;
const CGFloat kViewControllerHeight = 230.;
// Footer height for "Add Account…" section.
const CGFloat kFooterHeight = 17.;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  IdentitiesSectionIdentifier = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  IdentityItemType = kItemTypeEnumZero,
  AddAccountItemType,
};

}  // namespace

@implementation IdentityChooserViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];

  self.preferredContentSize =
      CGSizeMake(kViewControllerWidth, kViewControllerHeight);
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.contentInset = UIEdgeInsetsMake(0, 0, kFooterHeight, 0);
  self.tableView.sectionFooterHeight = 0;
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self.presentationDelegate identityChooserViewControllerDidDisappear:self];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  DCHECK_EQ(0, indexPath.section);
  ListItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch ((ItemType)item.type) {
    case IdentityItemType: {
      TableViewIdentityItem* tableViewIdentityItem =
          base::apple::ObjCCastStrict<TableViewIdentityItem>(item);
      DCHECK(tableViewIdentityItem);
      [self.presentationDelegate
          identityChooserViewController:self
            didSelectIdentityWithGaiaID:tableViewIdentityItem.gaiaID];
      break;
    }
    case AddAccountItemType:
      [self.presentationDelegate
          identityChooserViewControllerDidTapOnAddAccount:self];
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

#pragma mark - IdentityChooserConsumer

- (void)setIdentityItems:(NSArray<TableViewItem*>*)items {
  [self loadModel];

  TableViewModel* tableViewModel = self.tableViewModel;
  if ([tableViewModel
          hasSectionForSectionIdentifier:IdentitiesSectionIdentifier]) {
    [tableViewModel removeSectionWithIdentifier:IdentitiesSectionIdentifier];
  }
  [tableViewModel addSectionWithIdentifier:IdentitiesSectionIdentifier];
  // Create the header item.
  [tableViewModel setHeader:[[IdentityChooserHeaderItem alloc] init]
      forSectionWithIdentifier:IdentitiesSectionIdentifier];
  // Insert the items.
  for (TableViewItem* item in items) {
    item.type = IdentityItemType;
    [tableViewModel addItem:item
        toSectionWithIdentifier:IdentitiesSectionIdentifier];
  }
  // Insert "Add Account" item.
  IdentityChooserAddAccountItem* addAccountItem =
      [[IdentityChooserAddAccountItem alloc] initWithType:AddAccountItemType];
  [tableViewModel addItem:addAccountItem
      toSectionWithIdentifier:IdentitiesSectionIdentifier];

  [self.tableView reloadData];
}

- (void)itemHasChanged:(TableViewItem*)changedItem {
  if (![self.tableViewModel hasItem:changedItem])
    return;

  [self reconfigureCellsForItems:@[ changedItem ]];
}

- (TableViewIdentityItem*)tableViewIdentityItemWithGaiaID:(NSString*)gaiaID {
  for (TableViewIdentityItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:IdentitiesSectionIdentifier]) {
    if (item.type != IdentityItemType)
      continue;
    TableViewIdentityItem* identityItem =
        base::apple::ObjCCastStrict<TableViewIdentityItem>(item);
    if ([identityItem.gaiaID isEqualToString:gaiaID])
      return identityItem;
  }
  return nil;
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityPerformEscape {
  [self dismissViewControllerAnimated:YES completion:nil];
  return YES;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
