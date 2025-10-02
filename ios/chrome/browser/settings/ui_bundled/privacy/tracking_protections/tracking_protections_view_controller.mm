// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/tracking_protections_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/strings/grit/privacy_sandbox_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSettings = kSectionIdentifierEnumZero,
};

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeScriptBlocking = kItemTypeEnumZero,
  ItemTypeHeader,
};

NSString* const kTrackingProtectionsTableViewId =
    @"kTrackingProtectionsTableViewId";
NSString* const kTrackingProtectionsHeaderId = @"kTrackingProtectionsHeaderId";
NSString* const kScriptBlockingCellId = @"kScriptBlockingCellId";

const char kTrackingProtectionsHelpCenterURL[] =
    "https://support.google.com/chrome?p=incognito_tracking_protections";

}  // namespace

@implementation TrackingProtectionsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kTrackingProtectionsTableViewId;
  self.title =
      l10n_util::GetNSString(IDS_INCOGNITO_TRACKING_PROTECTIONS_PAGE_TITLE);
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate trackingProtectionsViewControllerDidRemove:self];
  }
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSettings];

  // Header.
  [model setHeader:[self headerItem]
      forSectionWithIdentifier:SectionIdentifierSettings];

  // Script blocking entrypoint.
  [model addItem:[self scriptBlockingItem]
      toSectionWithIdentifier:SectionIdentifierSettings];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  ItemType type = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  switch (type) {
    case ItemTypeScriptBlocking:
      [self.presentationDelegate
          trackingProtectionsViewControllerSelectedScriptBlocking:self];
      break;
    default:
      break;
  }
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForHeaderInSection:section];
  if (SectionIdentifierSettings ==
      [self.tableViewModel sectionIdentifierForSectionIndex:section]) {
    TableViewLinkHeaderFooterView* linkView =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    linkView.delegate = self;
  }
  return view;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/442799337): Record dismissal metric.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/442799337): Record back metric.
}

#pragma mark - Private

// Creates and returns the header item with description and link.
- (TableViewHeaderFooterItem*)headerItem {
  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.text = l10n_util::GetNSString(
      IDS_INCOGNITO_TRACKING_PROTECTIONS_DESCRIPTION_IOS);
  headerItem.urls =
      @[ [[CrURL alloc] initWithGURL:GURL(kTrackingProtectionsHelpCenterURL)] ];
  headerItem.accessibilityIdentifier = kTrackingProtectionsHeaderId;
  return headerItem;
}

// Creates and returns the item for the script blocking entrypoint.
- (TableViewMultiDetailTextItem*)scriptBlockingItem {
  TableViewMultiDetailTextItem* item = [[TableViewMultiDetailTextItem alloc]
      initWithType:ItemTypeScriptBlocking];
  item.text =
      l10n_util::GetNSString(IDS_FINGERPRINTING_PROTECTION_LINK_ROW_LABEL);
  item.leadingDetailText =
      l10n_util::GetNSString(IDS_FINGERPRINTING_PROTECTION_LINK_ROW_SUBLABEL);
  // TODO(crbug.com/442799337): Update to reflect script blocking pref state.
  item.trailingDetailText = nil;
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kScriptBlockingCellId;
  return item;
}

@end
