// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/bandwidth/bandwidth_management_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/bandwidth/dataplan_usage_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierActions = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePreload = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface BandwidthManagementTableViewController () <PrefObserverDelegate> {
  raw_ptr<ProfileIOS> _profile;  // weak

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrarApplicationContext;

  // Updatable Items
  TableViewDetailIconItem* _preloadWebpagesDetailItem;
}

@end

@implementation BandwidthManagementTableViewController

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS);
    _profile = profile;

    _prefChangeRegistrarApplicationContext.Init(_profile->GetPrefs());
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kNetworkPredictionSetting,
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

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierActions];
  [model addItem:[self preloadWebpagesItem]
      toSectionWithIdentifier:SectionIdentifierActions];

  [model setFooter:[self footerItem]
      forSectionWithIdentifier:SectionIdentifierActions];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileBandwidthSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileBandwidthSettingsBack"));
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  if (SectionIdentifierActions ==
      [self.tableViewModel sectionIdentifierForSectionIndex:section]) {
    TableViewLinkHeaderFooterView* footer =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(footerView);
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
        initWithPrefs:_profile->GetPrefs()
          settingPref:prefs::kNetworkPredictionSetting
                title:preloadTitle];
    [self.navigationController pushViewController:controller animated:YES];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kNetworkPredictionSetting) {
    NSString* detailText = [DataplanUsageTableViewController
        currentLabelForPreference:_profile->GetPrefs()
                      settingPref:prefs::kNetworkPredictionSetting];

    _preloadWebpagesDetailItem.detailText = detailText;

    [self reconfigureCellsForItems:@[ _preloadWebpagesDetailItem ]];
  }
}

#pragma mark - Private

// Returns a newly created TableViewDetailIconItem for the preload webpages
// menu.
- (TableViewDetailIconItem*)preloadWebpagesItem {
  NSString* detailText = [DataplanUsageTableViewController
      currentLabelForPreference:_profile->GetPrefs()
                    settingPref:prefs::kNetworkPredictionSetting];
  _preloadWebpagesDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypePreload];

  _preloadWebpagesDetailItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
  _preloadWebpagesDetailItem.detailText = detailText;
  _preloadWebpagesDetailItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  _preloadWebpagesDetailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  _preloadWebpagesDetailItem.accessibilityIdentifier = kSettingsPreloadCellId;
  return _preloadWebpagesDetailItem;
}

// Returns a newly created item for the footer of the section, describing how
// the bandwidth management is done.
- (TableViewLinkHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];

  item.text = l10n_util::GetNSString(
      IDS_IOS_BANDWIDTH_MANAGEMENT_DESCRIPTION_LEARN_MORE);
  item.urls = @[ [[CrURL alloc]
      initWithGURL:GURL(l10n_util::GetStringUTF8(
                       IDS_IOS_BANDWIDTH_MANAGEMENT_LEARN_URL))] ];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  return item;
}

@end
