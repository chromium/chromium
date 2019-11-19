// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/sync/sync_create_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierEncryption = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAccount = kItemTypeEnumZero,
  ItemTypePassphrase,
  ItemTypeFooter,
};

}  // namespace

@interface SyncEncryptionTableViewController () <SyncObserverModelBridge> {
  ios::ChromeBrowserState* _browserState;
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  BOOL _isUsingSecondaryPassphrase;
}
@end

@implementation SyncEncryptionTableViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_TITLE);
    _browserState = browserState;
    syncer::SyncService* syncService =
        ProfileSyncServiceFactory::GetForBrowserState(_browserState);
    _isUsingSecondaryPassphrase =
        syncService->IsEngineInitialized() &&
        syncService->GetUserSettings()->IsUsingSecondaryPassphrase();
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedSectionFooterHeight =
      kTableViewHeaderFooterViewHeight;
  [self loadModel];
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierEncryption];
  [model addItem:[self accountItem]
      toSectionWithIdentifier:SectionIdentifierEncryption];
  [model addItem:[self passphraseItem]
      toSectionWithIdentifier:SectionIdentifierEncryption];

  if (_isUsingSecondaryPassphrase) {
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:SectionIdentifierEncryption];
  }
}

#pragma mark - Items

// Returns an account item.
- (TableViewItem*)accountItem {
  DCHECK(switches::IsSyncAllowedByFlag());
  NSString* text = l10n_util::GetNSString(IDS_SYNC_BASIC_ENCRYPTION_DATA);
  return [self itemWithType:ItemTypeAccount
                       text:text
                    checked:!_isUsingSecondaryPassphrase
                    enabled:!_isUsingSecondaryPassphrase];
}

// Returns a passphrase item.
- (TableViewItem*)passphraseItem {
  DCHECK(switches::IsSyncAllowedByFlag());
  NSString* text = l10n_util::GetNSString(IDS_SYNC_FULL_ENCRYPTION_DATA);
  return [self itemWithType:ItemTypePassphrase
                       text:text
                    checked:_isUsingSecondaryPassphrase
                    enabled:!_isUsingSecondaryPassphrase];
}

// Returns a footer item with a link.
- (TableViewHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_PASSPHRASE_HINT);
  footerItem.linkURL = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  return footerItem;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  if (SectionIdentifierEncryption ==
          [self.tableViewModel sectionIdentifierForSection:section] &&
      [self.tableViewModel footerForSection:section]) {
    TableViewLinkHeaderFooterView* footer =
        base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(footerView);
    footer.delegate = self;
  }
  return footerView;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(indexPath.section,
            [self.tableViewModel
                sectionForSectionIdentifier:SectionIdentifierEncryption]);

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypePassphrase: {
      DCHECK(switches::IsSyncAllowedByFlag());
      syncer::SyncService* service =
          ProfileSyncServiceFactory::GetForBrowserState(_browserState);
      if (service->IsEngineInitialized() &&
          !service->GetUserSettings()->IsUsingSecondaryPassphrase()) {
        SyncCreatePassphraseTableViewController* controller =
            [[SyncCreatePassphraseTableViewController alloc]
                initWithBrowserState:_browserState];
        if (controller) {
          controller.dispatcher = self.dispatcher;
          [self.navigationController pushViewController:controller
                                               animated:YES];
        }
      }
      break;
    }
    case ItemTypeAccount:
    case ItemTypeFooter:
    default:
      break;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:NO];
}

#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  syncer::SyncService* service =
      ProfileSyncServiceFactory::GetForBrowserState(_browserState);
  BOOL isNowUsingSecondaryPassphrase =
      service->IsEngineInitialized() &&
      service->GetUserSettings()->IsUsingSecondaryPassphrase();
  if (_isUsingSecondaryPassphrase != isNowUsingSecondaryPassphrase) {
    _isUsingSecondaryPassphrase = isNowUsingSecondaryPassphrase;
    [self reloadData];
  }
}

#pragma mark - Private methods

- (TableViewItem*)itemWithType:(NSInteger)type
                          text:(NSString*)text
                       checked:(BOOL)checked
                       enabled:(BOOL)enabled {
  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:type];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.text = text;
  item.accessoryType = checked ? UITableViewCellAccessoryCheckmark
                               : UITableViewCellAccessoryNone;
  item.textColor =
      enabled ? UIColor.cr_labelColor : UIColor.cr_secondaryLabelColor;
  item.enabled = enabled;
  return item;
}

@end
