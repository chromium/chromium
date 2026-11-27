// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bandwidth/bandwidth_management_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/bandwidth/dataplan_usage_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
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
  ItemTypePreloadManaged,
  ItemTypeFooter,
};

}  // namespace

@interface BandwidthManagementTableViewController () <
    PopoverLabelViewControllerDelegate,
    PrefObserverDelegate> {
  raw_ptr<ProfileIOS> _profile;  // weak

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrarApplicationContext;

  // Updatable Items
  TableViewDetailIconItem* _preloadWebpagesDetailItem;
  // Managed "Preload webpages" row item shown when
  // `prefs::kNetworkPredictionSetting` is enforced by policy. `nil` when the
  // preference is unmanaged or after settings are dismissed.
  TableViewInfoButtonItem* _preloadWebpagesManagedItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@end

@implementation BandwidthManagementTableViewController

#pragma mark - Initialization

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  CHECK(profile);
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
  self.tableView.accessibilityIdentifier = kBandwidthSettingsTableViewId;
  self.tableView.estimatedRowHeight = 70;
  self.tableView.estimatedSectionFooterHeight = 70;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionFooterHeight = UITableViewAutomaticDimension;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  _preloadWebpagesDetailItem = nil;
  _preloadWebpagesManagedItem = nil;
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierActions];
  TableViewItem* preloadItem = nil;
  if (_profile->GetPrefs()->IsManagedPreference(
          prefs::kNetworkPredictionSetting)) {
    preloadItem = [self preloadWebpagesManagedItem];
  } else {
    preloadItem = [self preloadWebpagesItem];
  }
  [model addItem:preloadItem toSectionWithIdentifier:SectionIdentifierActions];

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

- (void)settingsWillBeDismissed {
  if (_settingsAreDismissed) {
    return;
  }

  [self dismissEnterprisePopoverAnimated:NO];
  _prefChangeRegistrarApplicationContext.Reset();
  _prefObserverBridge.reset();
  _preloadWebpagesDetailItem = nil;
  _preloadWebpagesManagedItem = nil;
  _profile = nullptr;
  _settingsAreDismissed = YES;
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

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_settingsAreDismissed ||
      [self.tableViewModel itemTypeForIndexPath:indexPath] ==
          ItemTypePreloadManaged) {
    return nil;
  }
  return [super tableView:tableView willSelectRowAtIndexPath:indexPath];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  if (_settingsAreDismissed ||
      [self.tableViewModel itemTypeForIndexPath:indexPath] != ItemTypePreload) {
    return;
  }
  PrefService* prefs = _profile->GetPrefs();
  if (prefs->IsManagedPreference(prefs::kNetworkPredictionSetting)) {
    return;
  }
  NSString* preloadTitle =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
  DataplanUsageTableViewController* controller =
      [[DataplanUsageTableViewController alloc]
          initWithPrefs:prefs
            settingPref:prefs::kNetworkPredictionSetting
                  title:preloadTitle];
  [self configureHandlersForRootViewController:controller];
  [self.navigationController pushViewController:controller animated:YES];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (_settingsAreDismissed ||
      preferenceName != prefs::kNetworkPredictionSetting) {
    return;
  }
  TableViewModel* tableViewModel = self.tableViewModel;
  if (!tableViewModel) {
    return;
  }

  PrefService* prefs = _profile->GetPrefs();
  BOOL isManaged = prefs->IsManagedPreference(prefs::kNetworkPredictionSetting);
  BOOL showingManagedItem =
      [tableViewModel hasItemForItemType:ItemTypePreloadManaged
                       sectionIdentifier:SectionIdentifierActions];
  if (isManaged != showingManagedItem) {
    if (!isManaged) {
      [self dismissEnterprisePopoverAnimated:YES];
    }
    [self reloadData];
    return;
  }

  NSString* detailText = [DataplanUsageTableViewController
      currentLabelForPreference:prefs
                    settingPref:prefs::kNetworkPredictionSetting];
  TableViewItem* updatedItem = nil;
  if (isManaged) {
    [self dismissEnterprisePopoverAnimated:NO];
    _preloadWebpagesManagedItem.statusText = detailText;
    updatedItem = _preloadWebpagesManagedItem;
  } else {
    _preloadWebpagesDetailItem.detailText = detailText;
    updatedItem = _preloadWebpagesDetailItem;
  }
  [self reconfigureCellsForItems:@[ updatedItem ]];
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
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

// Returns a newly created `TableViewInfoButtonItem` for the managed
// "Preload webpages" row.
- (TableViewInfoButtonItem*)preloadWebpagesManagedItem {
  _preloadWebpagesManagedItem =
      [[TableViewInfoButtonItem alloc] initWithType:ItemTypePreloadManaged];
  _preloadWebpagesManagedItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
  _preloadWebpagesManagedItem.statusText = [DataplanUsageTableViewController
      currentLabelForPreference:_profile->GetPrefs()
                    settingPref:prefs::kNetworkPredictionSetting];
  _preloadWebpagesManagedItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  _preloadWebpagesManagedItem.accessibilityIdentifier =
      kSettingsManagedPreloadCellId;
  _preloadWebpagesManagedItem.target = self;
  _preloadWebpagesManagedItem.selector = @selector(didTapManagedUIInfoButton:);
  return _preloadWebpagesManagedItem;
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

// Presents the enterprise info popover anchored to `buttonView` when the
// managed info button is tapped.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  if (_settingsAreDismissed || self.presentedViewController) {
    return;
  }

  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;
  buttonView.enabled = NO;
  UIPopoverPresentationController* popoverPresentationController =
      bubbleViewController.popoverPresentationController;
  popoverPresentationController.sourceView = buttonView;
  popoverPresentationController.sourceRect = buttonView.bounds;
  popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
  [self presentViewController:bubbleViewController animated:YES completion:nil];
}

// Dismisses the presented `EnterpriseInfoPopoverViewController`, if any, and
// re-enables its anchor info button.
- (void)dismissEnterprisePopoverAnimated:(BOOL)animated {
  UIViewController* presentedViewController = self.presentedViewController;
  if (![presentedViewController
          isKindOfClass:[EnterpriseInfoPopoverViewController class]]) {
    return;
  }

  base::apple::ObjCCastStrict<UIButton>(
      presentedViewController.popoverPresentationController.sourceView)
      .enabled = YES;
  [self dismissViewControllerAnimated:animated completion:nil];
}

@end
