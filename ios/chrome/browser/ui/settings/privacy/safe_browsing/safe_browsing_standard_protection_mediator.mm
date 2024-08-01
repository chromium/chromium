// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "build/branding_buildflags.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/ui/settings/cells/safe_browsing_header_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_consumer.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using ItemArray = NSArray<TableViewItem*>*;

namespace {

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeShieldIcon = kItemTypeEnumZero,
  ItemTypeMetricIcon,
  ItemTypePasswordLeakCheckSwitch,
  ItemTypeSafeBrowsingExtendedReporting,
  ItemTypeSafeBrowsingManagedExtendedReporting,
};

// The size of the symbols.
const CGFloat kSymbolSize = 20;

}  // namespace

@interface SafeBrowsingStandardProtectionMediator () <
    BooleanObserver,
    IdentityManagerObserverBridgeDelegate> {
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}

// User pref service used to check if a specific pref is managed by enterprise
// policies.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

// Authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;

// Returns YES if the user has selected Standard Protection for the Safe
// Browsing choice.
@property(nonatomic, assign, readonly) BOOL inSafeBrowsingStandardProtection;

// Preference value for the Safe Browsing Enhanced Protection feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingEnhancedProtectionPreference;

// Preference value for the "Safe Browsing" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingStandardProtectionPreference;

// Preference value for Safe Browsing Extended Reporting.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingExtendedReportingPreference;

// The observable boolean that binds to the password leak check settings
// state.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* passwordLeakCheckPreference;

// The item related to the switch for the automatic password leak detection
// setting.
@property(nonatomic, strong, null_resettable)
    TableViewSwitchItem* passwordLeakCheckItem;

// Header that has shield icon.
@property(nonatomic, strong) SafeBrowsingHeaderItem* shieldIconHeader;

// Second header which has a metric icon.
@property(nonatomic, strong) SafeBrowsingHeaderItem* metricIconHeader;

// All the items for the standard safe browsing section.
@property(nonatomic, strong, readonly)
    ItemArray safeBrowsingStandardProtectionItems;

@end

@implementation SafeBrowsingStandardProtectionMediator

@synthesize safeBrowsingStandardProtectionItems =
    _safeBrowsingStandardProtectionItems;

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                            authService:(AuthenticationService*)authService
                        identityManager:
                            (signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    _userPrefService = userPrefService;
    _authService = authService;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);

    _safeBrowsingEnhancedProtectionPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingEnhanced];
    _safeBrowsingEnhancedProtectionPreference.observer = self;
    _safeBrowsingStandardProtectionPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingEnabled];
    _safeBrowsingStandardProtectionPreference.observer = self;
    _safeBrowsingExtendedReportingPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingScoutReportingEnabled];
    _safeBrowsingExtendedReportingPreference.observer = self;
    _passwordLeakCheckPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:password_manager::prefs::
                                kPasswordLeakDetectionEnabled];
    _passwordLeakCheckPreference.observer = self;
  }
  return self;
}

- (void)disconnect {
  _identityManagerObserver = nil;
}

#pragma mark - Properties

- (ItemArray)safeBrowsingStandardProtectionItems {
  if (!_safeBrowsingStandardProtectionItems) {
    NSMutableArray* items = [NSMutableArray array];
    if (self.userPrefService->IsManagedPreference(
            prefs::kSafeBrowsingEnabled)) {
      TableViewInfoButtonItem* safeBrowsingManagedExtendedReportingItem = [self
          tableViewInfoButtonItemType:
              ItemTypeSafeBrowsingManagedExtendedReporting
                         textStringID:
                             IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_EXTENDED_REPORTING_TITLE
                       detailStringID:
                           IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_EXTENDED_REPORTING_SUMMARY
                               status:
                                   self.safeBrowsingStandardProtectionPreference
                                       .value];
      [items addObject:safeBrowsingManagedExtendedReportingItem];
    } else {
      SyncSwitchItem* safeBrowsingExtendedReportingItem = [self
          switchItemWithItemType:ItemTypeSafeBrowsingExtendedReporting
                    textStringID:
                        IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_EXTENDED_REPORTING_TITLE
                  detailStringID:
                      IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_EXTENDED_REPORTING_SUMMARY
                    defaultState:safe_browsing::IsExtendedReportingEnabled(
                                     *self.userPrefService)
                         enabled:self.inSafeBrowsingStandardProtection];
      safeBrowsingExtendedReportingItem.accessibilityIdentifier =
          kSafeBrowsingExtendedReportingCellId;
      [items addObject:safeBrowsingExtendedReportingItem];
    }
    [items addObject:self.passwordLeakCheckItem];

    _safeBrowsingStandardProtectionItems = items;
  }
  return _safeBrowsingStandardProtectionItems;
}

- (SafeBrowsingHeaderItem*)shieldIconHeader {
  if (!_shieldIconHeader) {
    UIImage* shieldIcon;
    shieldIcon = CustomSymbolWithPointSize(kPrivacySymbol, kSymbolSize);
    SafeBrowsingHeaderItem* shieldIconItem = [self
             detailItemWithType:ItemTypeShieldIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_BULLET_ONE
                          image:shieldIcon
        accessibilityIdentifier:kSafeBrowsingStandardProtectionShieldCellId];
    _shieldIconHeader = shieldIconItem;
  }
  return _shieldIconHeader;
}

- (SafeBrowsingHeaderItem*)metricIconHeader {
  if (!_metricIconHeader) {
    UIImage* metricIcon =
        DefaultSymbolWithPointSize(kCheckmarkCircleSymbol, kSymbolSize);
    NSInteger detailText = IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_BULLET_TWO;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (safe_browsing::hash_realtime_utils::
            IsHashRealTimeLookupEligibleInSession()) {
      detailText = IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_BULLET_TWO_PROXY;
    }
#endif

    SafeBrowsingHeaderItem* metricIconItem = [self
             detailItemWithType:ItemTypeMetricIcon
                     detailText:detailText
                          image:metricIcon
        accessibilityIdentifier:kSafeBrowsingStandardProtectionMetricCellId];
    _metricIconHeader = metricIconItem;
  }
  return _metricIconHeader;
}

- (TableViewSwitchItem*)passwordLeakCheckItem {
  if (!_passwordLeakCheckItem) {
    TableViewSwitchItem* passwordLeakCheckItem = [[TableViewSwitchItem alloc]
        initWithType:ItemTypePasswordLeakCheckSwitch];
    passwordLeakCheckItem.text = l10n_util::GetNSString(
        IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_LEAK_CHECK_TITLE);
    passwordLeakCheckItem.accessibilityIdentifier =
        kSafeBrowsingStandardProtectionPasswordLeakCellId;
    [self configureLeakCheckItem:passwordLeakCheckItem];

    _passwordLeakCheckItem = passwordLeakCheckItem;
  }
  return _passwordLeakCheckItem;
}

- (void)setConsumer:(id<SafeBrowsingStandardProtectionConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [_consumer setSafeBrowsingStandardProtectionItems:
                 self.safeBrowsingStandardProtectionItems];
  [_consumer setShieldIconHeader:self.shieldIconHeader];
  [_consumer setMetricIconHeader:self.metricIconHeader];
}

- (BOOL)inSafeBrowsingStandardProtection {
  return safe_browsing::GetSafeBrowsingState(*self.userPrefService) ==
         safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
}

#pragma mark - Private

// Creates header in Standard Protection view.
- (SafeBrowsingHeaderItem*)detailItemWithType:(NSInteger)type
                                   detailText:(NSInteger)detailText
                                        image:(UIImage*)image
                      accessibilityIdentifier:
                          (NSString*)accessibilityIdentifier {
  SafeBrowsingHeaderItem* detailItem =
      [[SafeBrowsingHeaderItem alloc] initWithType:type];
  detailItem.text = l10n_util::GetNSString(detailText);
  detailItem.image = image;
  detailItem.imageViewTintColor = [UIColor colorNamed:kGrey600Color];
  detailItem.accessibilityIdentifier = accessibilityIdentifier;

  return detailItem;
}

// Creates a TableViewInfoButtonItem instance used for items that the user is
// not allowed to switch on or off (enterprise reason for example).
- (TableViewInfoButtonItem*)tableViewInfoButtonItemType:(NSInteger)itemType
                                           textStringID:(int)textStringID
                                         detailStringID:(int)detailStringID
                                                 status:(BOOL)status {
  TableViewInfoButtonItem* managedItem =
      [[TableViewInfoButtonItem alloc] initWithType:itemType];
  managedItem.text = l10n_util::GetNSString(textStringID);
  managedItem.detailText = l10n_util::GetNSString(detailStringID);
  managedItem.statusText = status ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  if (!status) {
    managedItem.iconTintColor = [UIColor colorNamed:kGrey300Color];

    // This item is not controllable; set to lighter colors.
    managedItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    managedItem.detailTextColor = [UIColor colorNamed:kTextTertiaryColor];

    managedItem.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  }
  return managedItem;
}

// Creates an item with a switch toggle.
- (SyncSwitchItem*)switchItemWithItemType:(NSInteger)itemType
                             textStringID:(int)textStringID
                           detailStringID:(int)detailStringID
                             defaultState:(BOOL)defaultState
                                  enabled:(BOOL)enabled {
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = l10n_util::GetNSString(textStringID);
  if (detailStringID)
    switchItem.detailText = l10n_util::GetNSString(detailStringID);
  switchItem.on = defaultState;
  switchItem.enabled = enabled;
  return switchItem;
}

// Updates switches' on and enabled status.
- (void)updateSafeBrowsingStandardProtectionModel {
  for (TableViewItem* item in self.safeBrowsingStandardProtectionItems) {
    ItemType type = static_cast<ItemType>(item.type);
    switch (type) {
      case ItemTypePasswordLeakCheckSwitch:
        [self configureLeakCheckItem:item];
        break;
      case ItemTypeSafeBrowsingExtendedReporting: {
        SyncSwitchItem* syncSwitchItem =
            base::apple::ObjCCastStrict<SyncSwitchItem>(item);
        syncSwitchItem.on =
            safe_browsing::IsExtendedReportingEnabled(*self.userPrefService);
        syncSwitchItem.enabled = self.inSafeBrowsingStandardProtection;
        break;
      }
      case ItemTypeSafeBrowsingManagedExtendedReporting:
        base::apple::ObjCCastStrict<TableViewInfoButtonItem>(item).statusText =
            self.safeBrowsingExtendedReportingPreference.value
                ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
        break;
      default:
        break;
    }
  }

  [self.consumer reloadCellsForItems];
}

// Returns a boolean indicating if the switch should appear as "On" or "Off"
// based on the sync preference, the sign in status, and if the user has
// selected Standard Protection as the Safe Browsing option.
- (BOOL)passwordLeakCheckItemOnState {
  return self.safeBrowsingEnhancedProtectionPreference.value ||
         (self.safeBrowsingStandardProtectionPreference.value &&
          self.passwordLeakCheckPreference.value);
}

// Updates the detail text and on state of the leak check item based on the
// state.
- (void)configureLeakCheckItem:(TableViewItem*)item {
  TableViewSwitchItem* leakCheckItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(item);
  leakCheckItem.enabled = self.inSafeBrowsingStandardProtection;
  leakCheckItem.on = [self passwordLeakCheckItemOnState];
  leakCheckItem.detailText = l10n_util::GetNSString(
      IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_LEAK_CHECK_FRIENDLIER_SUMMARY);
}

#pragma mark - SafeBrowsingStandardProtectionViewControllerDelegate

- (void)toggleSwitchItem:(TableViewItem*)item withValue:(BOOL)value {
  SyncSwitchItem* syncSwitchItem = base::apple::ObjCCast<SyncSwitchItem>(item);
  syncSwitchItem.on = value;
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case ItemTypeSafeBrowsingExtendedReporting:
      self.safeBrowsingExtendedReportingPreference.value = value;
      break;
    case ItemTypePasswordLeakCheckSwitch:
      self.passwordLeakCheckPreference.value = value;
      break;
    default:
      // Not a switch.
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateSafeBrowsingStandardProtectionModel];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Used to update model when signing in/out is completed.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self updateSafeBrowsingStandardProtectionModel];
}

@end
