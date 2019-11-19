// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/bandwidth_management_table_view_controller.h"

#include "base/mac/foundation_util.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/settings/dataplan_usage_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierActions = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePreload = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface BandwidthManagementTableViewController ()<PrefObserverDelegate> {
  ios::ChromeBrowserState* _browserState;  // weak

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrarApplicationContext;

  // Updatable Items
  TableViewDetailIconItem* _preloadWebpagesDetailItem;
}

@end

@implementation BandwidthManagementTableViewController

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS);
    _browserState = browserState;

    _prefChangeRegistrarApplicationContext.Init(_browserState->GetPrefs());
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kNetworkPredictionEnabled,
        &_prefChangeRegistrarApplicationContext);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kNetworkPredictionWifiOnly,
        &_prefChangeRegistrarApplicationContext);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedRowHeight = 70;
  self.tableView.estimatedSectionFooterHeight = 70;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionFooterHeight = UITableViewAutomaticDimension;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierActions];
  [model addItem:[self preloadWebpagesItem]
      toSectionWithIdentifier:SectionIdentifierActions];

  [model setFooter:[self footerItem]
      forSectionWithIdentifier:SectionIdentifierActions];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView =
      [super tableView:tableView viewForFooterInSection:section];
  if (SectionIdentifierActions ==
      [self.tableViewModel sectionIdentifierForSection:section]) {
    TableViewLinkHeaderFooterView* footer =
        base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(footerView);
    footer.delegate = self;
  }
  return footerView;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypePreload) {
    NSString* preloadTitle =
        l10n_util::GetNSString(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
    UIViewController* controller = [[DataplanUsageTableViewController alloc]
        initWithPrefs:_browserState->GetPrefs()
             basePref:prefs::kNetworkPredictionEnabled
             wifiPref:prefs::kNetworkPredictionWifiOnly
                title:preloadTitle];
    [self.navigationController pushViewController:controller animated:YES];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kNetworkPredictionEnabled ||
      preferenceName == prefs::kNetworkPredictionWifiOnly) {
    NSString* detailText = [DataplanUsageTableViewController
        currentLabelForPreference:_browserState->GetPrefs()
                         basePref:prefs::kNetworkPredictionEnabled
                         wifiPref:prefs::kNetworkPredictionWifiOnly];

    _preloadWebpagesDetailItem.detailText = detailText;

    [self reconfigureCellsForItems:@[ _preloadWebpagesDetailItem ]];
  }
}

#pragma mark - Private

// Returns a newly created TableViewDetailIconItem for the preload webpages
// menu.
- (TableViewDetailIconItem*)preloadWebpagesItem {
  NSString* detailText = [DataplanUsageTableViewController
      currentLabelForPreference:_browserState->GetPrefs()
                       basePref:prefs::kNetworkPredictionEnabled
                       wifiPref:prefs::kNetworkPredictionWifiOnly];
  _preloadWebpagesDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypePreload];

  _preloadWebpagesDetailItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
  _preloadWebpagesDetailItem.detailText = detailText;
  _preloadWebpagesDetailItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _preloadWebpagesDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return _preloadWebpagesDetailItem;
}

// Returns a newly created item for the footer of the section, describing how
// the bandwidth management is done.
- (TableViewLinkHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];

  item.text = l10n_util::GetNSString(
      IDS_IOS_BANDWIDTH_MANAGEMENT_DESCRIPTION_LEARN_MORE);
  item.linkURL =
      GURL(l10n_util::GetStringUTF8(IDS_IOS_BANDWIDTH_MANAGEMENT_LEARN_URL));
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

@end
