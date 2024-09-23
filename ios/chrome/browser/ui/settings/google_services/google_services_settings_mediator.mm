// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/auto_reset.h"
#import "base/feature_list.h"
#import "base/notreached.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/browser/supervised_user_capabilities.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/common/features.h"
#import "components/sync/service/sync_service.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

typedef NSArray<TableViewItem*>* ItemArray;

namespace {

NSString* const kBetterSearchAndBrowsingItemAccessibilityID =
    @"betterSearchAndBrowsingItem_switch";
NSString* const kTrackPricesOnTabsItemAccessibilityID =
    @"trackPricesOnTabsItem_switch";

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  NonPersonalizedSectionIdentifier = kSectionIdentifierEnumZero,
  ParcelTrackingSectionIdentifier
};

// List of items. For implementation details in
// GoogleServicesSettingsViewController, two SyncSwitchItem items should not
// share the same type. The cell UISwitch tag is used to save the item type, and
// when the user taps on the switch, this tag is used to retrieve the item
// based on the type.
typedef NS_ENUM(NSInteger, ItemType) {
  AllowChromeSigninItemType = kItemTypeEnumZero,
  ImproveChromeItemType,
  ImproveChromeManagedItemType,
  BetterSearchAndBrowsingItemType,
  BetterSearchAndBrowsingManagedItemType,
  ImproveSearchSuggestionsItemType,
  ImproveSearchSuggestionsManagedItemType,
  TrackPricesOnTabsItemType,
  ParcelTrackingItemType,
};

// TODO(crbug.com/40788009): Use the Authentication Service sign-in status API
// instead of this when available.
// Returns true when sign-in can be enabled/disabled by the user from the
// google service settings.
bool IsControllingSigninAllowedByPolicy() {
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      GetApplicationContext()->GetLocalState()->GetInteger(
          prefs::kBrowserSigninPolicy));
  switch (policy_mode) {
    case BrowserSigninMode::kEnabled:
      return true;
    case BrowserSigninMode::kDisabled:
    case BrowserSigninMode::kForced:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool GetStatusForSigninPolicy() {
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      GetApplicationContext()->GetLocalState()->GetInteger(
          prefs::kBrowserSigninPolicy));
  switch (policy_mode) {
    case BrowserSigninMode::kEnabled:
    case BrowserSigninMode::kForced:
      return true;
    case BrowserSigninMode::kDisabled:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace

@interface GoogleServicesSettingsMediator () <BooleanObserver>

// Returns YES if the user is authenticated.
@property(nonatomic, assign, readonly) BOOL hasPrimaryIdentity;
// ** Non personalized section.
// Preference value for the "Allow Chrome Sign-in" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* allowChromeSigninPreference;
// Preference value for the "Help improve Chromium's features" for Wifi-Only.
// TODO(crbug.com/40588486): Needs to create the UI to change from Wifi-Only to
// always
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* sendDataUsageWifiOnlyPreference;
// Preference value for the "Make searches and browsing better" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* anonymizedDataCollectionPreference;
// Preference value for the "Improve search suggestions" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* improveSearchSuggestionsPreference;
// Preference value for the "Help improve Chromium's features" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* sendDataUsagePreference;

// All the items for the non-personalized section.
@property(nonatomic, strong, readonly) ItemArray nonPersonalizedItems;

// User pref service used to check if a specific pref is managed by enterprise
// policies.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

// Local pref service used to check if a specific pref is managed by enterprise
// policies.
@property(nonatomic, assign, readonly) PrefService* localPrefService;

// Preference value for displaying price drop annotations on Tabs for shopping
// URLs in the Tab Switching UI as price drops are identified.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* trackPricesOnTabsPreference;

@end

@implementation GoogleServicesSettingsMediator

@synthesize nonPersonalizedItems = _nonPersonalizedItems;

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                        userPrefService:(PrefService*)userPrefService
                       localPrefService:(PrefService*)localPrefService {
  self = [super init];
  if (self) {
    DCHECK(identityManager);
    DCHECK(userPrefService);
    DCHECK(localPrefService);
    _identityManager = identityManager;
    _userPrefService = userPrefService;
    _localPrefService = localPrefService;
    _allowChromeSigninPreference =
        [[PrefBackedBoolean alloc] initWithPrefService:userPrefService
                                              prefName:prefs::kSigninAllowed];
    _allowChromeSigninPreference.observer = self;
    _sendDataUsagePreference = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:metrics::prefs::kMetricsReportingEnabled];
    _sendDataUsagePreference.observer = self;
    _anonymizedDataCollectionPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:unified_consent::prefs::
                                kUrlKeyedAnonymizedDataCollectionEnabled];
    _anonymizedDataCollectionPreference.observer = self;
    _improveSearchSuggestionsPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSearchSuggestEnabled];
    _improveSearchSuggestionsPreference.observer = self;
    _trackPricesOnTabsPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kTrackPricesOnTabsEnabled];
    _trackPricesOnTabsPreference.observer = self;
  }
  return self;
}

- (TableViewItem*)allowChromeSigninItem {
  // Supervised users cannot manually enable/disable sign-in.
  if (![self isSubjectToParentalControls] &&
      IsControllingSigninAllowedByPolicy()) {
    return
        [self switchItemWithItemType:AllowChromeSigninItemType
                        textStringID:
                            IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_TEXT
                      detailStringID:
                          IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_DETAIL];
  }
  // Disables "Allow Chrome Sign-in" switch with a disclosure that the
  // setting has been disabled by the organization.
  return [self
      tableViewInfoButtonItemType:AllowChromeSigninItemType
                     textStringID:
                         IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_TEXT
                   detailStringID:
                       IDS_IOS_GOOGLE_SERVICES_SETTINGS_ALLOW_SIGNIN_DETAIL
                           status:GetStatusForSigninPolicy()];
}

#pragma mark - Load non personalized section

// Loads NonPersonalizedSectionIdentifier section.
- (void)loadNonPersonalizedSection {
  TableViewModel* model = self.consumer.tableViewModel;
  [model addSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  for (TableViewItem* item in self.nonPersonalizedItems) {
    [model addItem:item
        toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  }
  [self updateNonPersonalizedSectionWithNotification:NO];
}

// Updates the non-personalized section according to the user consent. If
// `notifyConsumer` is YES, the consumer is notified about model changes.
- (void)updateNonPersonalizedSectionWithNotification:(BOOL)notifyConsumer {
  for (TableViewItem* item in self.nonPersonalizedItems) {
    ItemType type = static_cast<ItemType>(item.type);
    switch (type) {
      case AllowChromeSigninItemType: {
        SyncSwitchItem* signinDisabledItem =
            base::apple::ObjCCast<SyncSwitchItem>(item);
        // Supervised users cannot manually enable/disable sign-in.
        if (![self isSubjectToParentalControls] &&
            IsControllingSigninAllowedByPolicy()) {
          signinDisabledItem.on = self.allowChromeSigninPreference.value;
        } else {
          signinDisabledItem.on = NO;
          signinDisabledItem.enabled = NO;
        }
        break;
      }
      case ImproveChromeItemType:
        base::apple::ObjCCast<SyncSwitchItem>(item).on =
            self.sendDataUsagePreference.value;
        break;
      case ImproveChromeManagedItemType:
        base::apple::ObjCCast<TableViewInfoButtonItem>(item).statusText =
            self.sendDataUsagePreference.value
                ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
        break;
      case BetterSearchAndBrowsingItemType:
        base::apple::ObjCCast<SyncSwitchItem>(item).on =
            self.anonymizedDataCollectionPreference.value;
        break;
      case BetterSearchAndBrowsingManagedItemType:
        base::apple::ObjCCast<TableViewInfoButtonItem>(item).statusText =
            self.anonymizedDataCollectionPreference.value
                ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
        break;
      case ImproveSearchSuggestionsItemType:
        base::apple::ObjCCast<SyncSwitchItem>(item).on =
            self.improveSearchSuggestionsPreference.value;
        break;
      case ImproveSearchSuggestionsManagedItemType:
        base::apple::ObjCCast<TableViewInfoButtonItem>(item).statusText =
            self.improveSearchSuggestionsPreference.value
                ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
        break;
      case TrackPricesOnTabsItemType:
        base::apple::ObjCCast<SyncSwitchItem>(item).on =
            self.trackPricesOnTabsPreference.value;
        break;
      case ParcelTrackingItemType:
        break;
    }
  }
  if (notifyConsumer) {
    TableViewModel* model = self.consumer.tableViewModel;
    NSUInteger sectionIndex =
        [model sectionForSectionIdentifier:NonPersonalizedSectionIdentifier];
    NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
    [self.consumer reloadSections:indexSet];
  }
}

#pragma mark - Properties

- (BOOL)hasPrimaryIdentity {
  return self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

- (ItemArray)nonPersonalizedItems {
  if (!_nonPersonalizedItems) {
    NSMutableArray* items = [NSMutableArray array];

    TableViewItem* allowSigninItem = [self allowChromeSigninItem];
    allowSigninItem.accessibilityIdentifier =
        kAllowSigninItemAccessibilityIdentifier;
    [items addObject:allowSigninItem];

    if (self.localPrefService->IsManagedPreference(
            metrics::prefs::kMetricsReportingEnabled) &&
        !self.localPrefService->GetBoolean(
            metrics::prefs::kMetricsReportingEnabled)) {
      TableViewInfoButtonItem* improveChromeItem = [self
          tableViewInfoButtonItemType:ImproveChromeManagedItemType
                         textStringID:
                             IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
                       detailStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL
                               status:self.sendDataUsagePreference];
      [items addObject:improveChromeItem];
    } else {
      SyncSwitchItem* improveChromeItem = [self
          switchItemWithItemType:ImproveChromeItemType
                    textStringID:
                        IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
                  detailStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL];
      improveChromeItem.accessibilityIdentifier =
          kImproveChromeItemAccessibilityIdentifier;
      [items addObject:improveChromeItem];
    }
    if (self.userPrefService->IsManagedPreference(
            unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled)) {
      TableViewInfoButtonItem* betterSearchAndBrowsingItem = [self
          tableViewInfoButtonItemType:BetterSearchAndBrowsingManagedItemType
                         textStringID:
                             IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
                       detailStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL
                               status:self.anonymizedDataCollectionPreference];
      betterSearchAndBrowsingItem.accessibilityIdentifier =
          kBetterSearchAndBrowsingItemAccessibilityID;
      [items addObject:betterSearchAndBrowsingItem];
    } else {
      SyncSwitchItem* betterSearchAndBrowsingItem = [self
          switchItemWithItemType:BetterSearchAndBrowsingItemType
                    textStringID:
                        IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
                  detailStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL];
      betterSearchAndBrowsingItem.accessibilityIdentifier =
          kBetterSearchAndBrowsingItemAccessibilityID;
      [items addObject:betterSearchAndBrowsingItem];
    }
    if (self.userPrefService->IsManagedPreference(
            prefs::kSearchSuggestEnabled)) {
      TableViewInfoButtonItem* improveSearchSuggestionsItem = [self
          tableViewInfoButtonItemType:ImproveSearchSuggestionsManagedItemType
                         textStringID:
                             IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_SEARCH_SUGGESTIONS_TEXT
                       detailStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_SEARCH_SUGGESTIONS_DETAIL
                               status:self.improveSearchSuggestionsPreference
                                          .value];
      [items addObject:improveSearchSuggestionsItem];
    } else {
      SyncSwitchItem* improveSearchSuggestionsItem = [self
          switchItemWithItemType:ImproveSearchSuggestionsItemType
                    textStringID:
                        IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_SEARCH_SUGGESTIONS_TEXT
                  detailStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_SEARCH_SUGGESTIONS_DETAIL];
      [items addObject:improveSearchSuggestionsItem];
    }
    if (self.userPrefService->IsManagedPreference(
            prefs::kTrackPricesOnTabsEnabled)) {
      TableViewInfoButtonItem* trackPricesOnTabsItem = [self
          tableViewInfoButtonItemType:TrackPricesOnTabsItemType
                         textStringID:IDS_IOS_TRACK_PRICES_ON_TABS
                       detailStringID:IDS_IOS_TRACK_PRICES_ON_TABS_DESCRIPTION
                               status:self.trackPricesOnTabsPreference];
      trackPricesOnTabsItem.accessibilityIdentifier =
          kTrackPricesOnTabsItemAccessibilityID;
      [items addObject:trackPricesOnTabsItem];
    } else {
      SyncSwitchItem* trackPricesOnTabsItem = [self
          switchItemWithItemType:TrackPricesOnTabsItemType
                    textStringID:IDS_IOS_TRACK_PRICES_ON_TABS
                  detailStringID:IDS_IOS_TRACK_PRICES_ON_TABS_DESCRIPTION];
      trackPricesOnTabsItem.accessibilityIdentifier =
          kTrackPricesOnTabsItemAccessibilityID;
      [items addObject:trackPricesOnTabsItem];
    }
    _nonPersonalizedItems = items;
  }
  return _nonPersonalizedItems;
}

#pragma mark - Private

// Creates an item with a switch toggle.
- (SyncSwitchItem*)switchItemWithItemType:(NSInteger)itemType
                             textStringID:(int)textStringID
                           detailStringID:(int)detailStringID {
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = GetNSString(textStringID);
  if (detailStringID)
    switchItem.detailText = GetNSString(detailStringID);
  return switchItem;
}

// Create a TableViewInfoButtonItem instance used for items that the user is
// not allowed to switch on or off (enterprise reason for example).
- (TableViewInfoButtonItem*)tableViewInfoButtonItemType:(NSInteger)itemType
                                           textStringID:(int)textStringID
                                         detailStringID:(int)detailStringID
                                                 status:(BOOL)status {
  TableViewInfoButtonItem* managedItem =
      [[TableViewInfoButtonItem alloc] initWithType:itemType];
  managedItem.text = GetNSString(textStringID);
  managedItem.detailText = GetNSString(detailStringID);
  managedItem.statusText = status ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  if (!status) {
    managedItem.iconTintColor = [UIColor colorNamed:kGrey300Color];
  }

  // This item is not controllable; set to lighter colors.
  managedItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  managedItem.detailTextColor = [UIColor colorNamed:kTextTertiaryColor];
  managedItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  return managedItem;
}

#pragma mark - GoogleServicesSettingsViewControllerModelDelegate

- (void)googleServicesSettingsViewControllerLoadModel:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.consumer, controller);
  [self loadNonPersonalizedSection];

  if (IsIOSParcelTrackingEnabled()) {
    TableViewModel* model = self.consumer.tableViewModel;
    [model addSectionWithIdentifier:ParcelTrackingSectionIdentifier];

    TableViewDetailIconItem* parcelTrackingItem =
        [[TableViewDetailIconItem alloc] initWithType:ParcelTrackingItemType];
    parcelTrackingItem.text = l10n_util::GetNSString(
        IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE);
    parcelTrackingItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
    parcelTrackingItem.accessibilityTraits |= UIAccessibilityTraitButton;
    [model addItem:parcelTrackingItem
        toSectionWithIdentifier:ParcelTrackingSectionIdentifier];

    IOSParcelTrackingOptInStatus optInStatus =
        static_cast<IOSParcelTrackingOptInStatus>(
            self.userPrefService->GetInteger(
                prefs::kIosParcelTrackingOptInStatus));
    NSString* currentOptInStatusString = nil;
    switch (optInStatus) {
      case IOSParcelTrackingOptInStatus::kAlwaysTrack:
        currentOptInStatusString = l10n_util::GetNSString(
            IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTO_TRACK_PACKAGES_ALL);
        break;
      case IOSParcelTrackingOptInStatus::kAskToTrack:
      case IOSParcelTrackingOptInStatus::kStatusNotSet:
        currentOptInStatusString = l10n_util::GetNSString(
            IDS_IOS_PARCEL_TRACKING_OPT_IN_TERTIARY_ACTION);
        break;
      case IOSParcelTrackingOptInStatus::kNeverTrack:
        currentOptInStatusString = l10n_util::GetNSString(
            IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTO_TRACK_PACKAGES_NEVER);
        break;
    }
    parcelTrackingItem.detailText = currentOptInStatusString;
  }
}

- (BOOL)isAllowChromeSigninItem:(int)type {
  return type == AllowChromeSigninItemType;
}

- (BOOL)isViewControllerSubjectToParentalControls {
  return [self isSubjectToParentalControls];
}

- (void)googleServicesSettingsViewControllerDidSelectItemAtIndexPath:
    (NSIndexPath*)indexPath {
  if ([self.consumer.tableViewModel itemAtIndexPath:indexPath].type ==
      ParcelTrackingItemType) {
    [self.commandHandler showParcelTrackingSettingsPage];
  }
}

#pragma mark - GoogleServicesSettingsServiceDelegate

- (void)toggleSwitchItem:(TableViewItem*)item
               withValue:(BOOL)value
              targetRect:(CGRect)targetRect {
  SyncSwitchItem* syncSwitchItem = base::apple::ObjCCast<SyncSwitchItem>(item);
  syncSwitchItem.on = value;
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case AllowChromeSigninItemType: {
      if (self.hasPrimaryIdentity) {
        __weak GoogleServicesSettingsMediator* weakSelf = self;
        [self.commandHandler
            showSignOutFromTargetRect:targetRect
                           completion:^(BOOL success) {
                             weakSelf.allowChromeSigninPreference.value =
                                 success ? value : !value;
                             [weakSelf
                                 updateNonPersonalizedSectionWithNotification:
                                     YES];
                           }];
      } else {
        self.allowChromeSigninPreference.value = value;
      }
      break;
    }
    case ImproveChromeItemType:
      self.sendDataUsagePreference.value = value;
      // Don't set value if sendDataUsageWifiOnlyPreference has not been
      // allocated.
      if (value && self.sendDataUsageWifiOnlyPreference) {
        // Should be wifi only, until https://crbug.com/872101 is fixed.
        self.sendDataUsageWifiOnlyPreference.value = YES;
      }
      break;
    case BetterSearchAndBrowsingItemType:
      self.anonymizedDataCollectionPreference.value = value;
      break;
    case ImproveSearchSuggestionsItemType:
      self.improveSearchSuggestionsPreference.value = value;
      break;
    case TrackPricesOnTabsItemType:
      self.trackPricesOnTabsPreference.value = value;
      break;
    case BetterSearchAndBrowsingManagedItemType:
    case ImproveChromeManagedItemType:
    case ImproveSearchSuggestionsManagedItemType:
      NOTREACHED_IN_MIGRATION();
      break;
    case ParcelTrackingItemType:
      break;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateNonPersonalizedSectionWithNotification:YES];
}

#pragma mark - Private

- (BOOL)isSubjectToParentalControls {
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS)) {
    return self.identityManager &&
           supervised_user::IsPrimaryAccountSubjectToParentalControls(
               self.identityManager) == signin::Tribool::kTrue;
  } else {
    return self.userPrefService &&
           supervised_user::IsSubjectToParentalControls(*self.userPrefService);
  }
}

@end
