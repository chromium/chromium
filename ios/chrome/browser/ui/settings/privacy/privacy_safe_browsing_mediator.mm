// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/auto_reset.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "build/branding_buildflags.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item_delegate.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_consumer.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_navigation_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using ItemArray = NSArray<TableViewItem*>*;

namespace {

// The size of the symbol image.
CGFloat kSymbolImagePointSize = 17.;

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSafeBrowsingStandardProtection = kItemTypeEnumZero,
  ItemTypeSafeBrowsingEnhancedProtection,
  ItemTypeSafeBrowsingNoProtection,
};

}  // namespace

@interface PrivacySafeBrowsingMediator () <BooleanObserver,
                                           TableViewInfoButtonItemDelegate>

// Preference value for the enhanced safe browsing feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingEnhancedProtectionPreference;

// Preference value for the standard safe browsing feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingStandardProtectionPreference;

// All the items for the safe browsing section.
@property(nonatomic, strong, readonly) ItemArray safeBrowsingItems;

// User pref service used to check if a specific pref is managed by enterprise
// policies.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

// Boolean to check if safe browsing is controlled by enterprise.
@property(nonatomic, readonly) BOOL enterpriseEnabled;

@end

@implementation PrivacySafeBrowsingMediator

@synthesize safeBrowsingItems = _safeBrowsingItems;

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    _userPrefService = userPrefService;
    _safeBrowsingEnhancedProtectionPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingEnhanced];
    _safeBrowsingEnhancedProtectionPreference.observer = self;
    _safeBrowsingStandardProtectionPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingEnabled];
    _safeBrowsingStandardProtectionPreference.observer = self;
  }
  return self;
}

- (void)selectSettingItem:(TableViewItem*)item {
  // If item is already selected or if we cancel turning off Safe Browsing,
  // don't do anything and keep the current selected choice.
  ItemType type = static_cast<ItemType>(item.type);
  if (item == nil || [self shouldItemTypeHaveCheckmark:type]) {
    [self updatePrivacySafeBrowsingSectionAndNotifyConsumer:YES];
    return;
  }

  // Show checkmark for selected item and update associated preference value by
  // setting the SafeBrowsingState.
  safe_browsing::SafeBrowsingState safeBrowsingState =
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING;
  switch (type) {
    case ItemTypeSafeBrowsingEnhancedProtection:
      safeBrowsingState = safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION;
      break;
    case ItemTypeSafeBrowsingStandardProtection:
      safeBrowsingState = safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
      break;
    case ItemTypeSafeBrowsingNoProtection:
      safeBrowsingState = safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  safe_browsing::SetSafeBrowsingState(self.userPrefService, safeBrowsingState);

  [self updatePrivacySafeBrowsingSectionAndNotifyConsumer:YES];
}

#pragma mark - Properties

- (ItemArray)safeBrowsingItems {
  if (!_safeBrowsingItems) {
    NSMutableArray* items = [NSMutableArray array];
    NSInteger enhancedProtectionSummary;
    enhancedProtectionSummary =
        IDS_IOS_PRIVACY_SAFE_BROWSING_ENHANCED_PROTECTION_FRIENDLIER_SUMMARY;
    NSInteger standardProtectionSummary;
      standardProtectionSummary =
          IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_FRIENDLIER_SUMMARY;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      if (safe_browsing::hash_realtime_utils::
              IsHashRealTimeLookupEligibleInSession()) {
        standardProtectionSummary =
            IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_FRIENDLIER_SUMMARY_PROXY;
      }
#endif
    TableViewInfoButtonItem* safeBrowsingEnhancedProtectionItem = [self
             infoButtonItemType:ItemTypeSafeBrowsingEnhancedProtection
                        titleId:
                            IDS_IOS_PRIVACY_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE
                     detailText:enhancedProtectionSummary
        accessibilityIdentifier:kSettingsSafeBrowsingEnhancedProtectionCellId];
    [items addObject:safeBrowsingEnhancedProtectionItem];

    TableViewInfoButtonItem* safeBrowsingStandardProtectionItem = [self
             infoButtonItemType:ItemTypeSafeBrowsingStandardProtection
                        titleId:
                            IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE
                     detailText:standardProtectionSummary
        accessibilityIdentifier:kSettingsSafeBrowsingStandardProtectionCellId];
    [items addObject:safeBrowsingStandardProtectionItem];
    NSInteger noProtectionSummary;
    noProtectionSummary =
        IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_FRIENDLIER_SUMMARY;
    if (self.enterpriseEnabled) {
      TableViewInfoButtonItem* safeBrowsingNoProtectionItem = [self
               infoButtonItemType:ItemTypeSafeBrowsingNoProtection
                          titleId:
                              IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_TITLE
                       detailText:noProtectionSummary
          accessibilityIdentifier:kSettingsSafeBrowsingNoProtectionCellId];
      [items addObject:safeBrowsingNoProtectionItem];
    } else {
      TableViewInfoButtonItem* safeBrowsingNoProtectionItem = [self
               infoButtonItemType:ItemTypeSafeBrowsingNoProtection
                          titleId:
                              IDS_IOS_PRIVACY_SAFE_BROWSING_NO_PROTECTION_TITLE
                       detailText:noProtectionSummary
          accessibilityIdentifier:kSettingsSafeBrowsingNoProtectionCellId];
      safeBrowsingNoProtectionItem.infoButtonIsHidden = YES;
      [items addObject:safeBrowsingNoProtectionItem];
    }
    _safeBrowsingItems = items;
  }
  return _safeBrowsingItems;
}

- (void)setConsumer:(id<PrivacySafeBrowsingConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [_consumer setSafeBrowsingItems:self.safeBrowsingItems];
  [_consumer setEnterpriseEnabled:self.enterpriseEnabled];
}

- (BOOL)enterpriseEnabled {
  return self.userPrefService->IsManagedPreference(prefs::kSafeBrowsingEnabled);
}

#pragma mark - Private

// Creates item with an image checkmark and an info button.
- (TableViewInfoButtonItem*)infoButtonItemType:(NSInteger)type
                                       titleId:(NSInteger)titleId
                                    detailText:(NSInteger)detailText
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewInfoButtonItem* infoButtonItem =
      [[TableViewInfoButtonItem alloc] initWithType:type];
  infoButtonItem.text = l10n_util::GetNSString(titleId);
  infoButtonItem.detailText = l10n_util::GetNSString(detailText);
  // If Safe Browsing is controlled by enterprise, make non-selected options
  // greyed out.
  if (self.enterpriseEnabled && ![self shouldItemTypeHaveCheckmark:type]) {
    // This item is not controllable; set to lighter colors.
    infoButtonItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    infoButtonItem.detailTextColor = [UIColor colorNamed:kTextTertiaryColor];
    infoButtonItem.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  } else {
    infoButtonItem.accessibilityActivationPointOnButton = NO;
    infoButtonItem.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_TABLE_VIEW_INFO_BUTTON_ITEM_ACCESSIBILITY_TAP);
  }
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolImagePointSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
  infoButtonItem.iconImage =
      DefaultSymbolWithConfiguration(kCheckmarkSymbol, configuration);
  infoButtonItem.iconTintColor = [self shouldItemTypeHaveCheckmark:type]
                                     ? [UIColor colorNamed:kBlueColor]
                                     : [UIColor clearColor];
  infoButtonItem.accessibilityIdentifier = accessibilityIdentifier;
  infoButtonItem.accessibilityDelegate = self;

  return infoButtonItem;
}

// Returns whether an ItemType should have a checkmark based on its
// SafeBrowsingState.
- (BOOL)shouldItemTypeHaveCheckmark:(NSInteger)itemType {
  ItemType type = static_cast<ItemType>(itemType);
  safe_browsing::SafeBrowsingState safeBrowsingState =
      safe_browsing::GetSafeBrowsingState(*self.userPrefService);
  switch (type) {
    case ItemTypeSafeBrowsingEnhancedProtection:
      return safeBrowsingState ==
             safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION;
    case ItemTypeSafeBrowsingStandardProtection:
      return safeBrowsingState ==
             safe_browsing::SafeBrowsingState::STANDARD_PROTECTION;
    case ItemTypeSafeBrowsingNoProtection:
      return safeBrowsingState ==
             safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING;
    default:
      NOTREACHED_IN_MIGRATION();
      return NO;
  }
}

// Updates the privacy safe browsing section according to the user consent. If
// `notifyConsumer` is YES, the consumer is notified about model changes.
- (void)updatePrivacySafeBrowsingSectionAndNotifyConsumer:(BOOL)notifyConsumer {
  for (TableViewItem* item in self.safeBrowsingItems) {
    TableViewInfoButtonItem* infoButtonItem =
        base::apple::ObjCCast<TableViewInfoButtonItem>(item);
    ItemType type = static_cast<ItemType>(item.type);
    infoButtonItem.iconTintColor = [self shouldItemTypeHaveCheckmark:type]
                                       ? [UIColor colorNamed:kBlueColor]
                                       : [UIColor clearColor];
  }

  if (notifyConsumer) {
    [self.consumer reconfigureCellsForItems];
    [self selectItem];
  }
}

// Check if selected row should display enterprise popover.
- (BOOL)shouldEnterprisePopOverDisplay:(TableViewItem*)item {
  ItemType type = static_cast<ItemType>(item.type);
  return self.enterpriseEnabled && (![self shouldItemTypeHaveCheckmark:type] ||
                                    type == ItemTypeSafeBrowsingNoProtection);
}

#pragma mark - SafeBrowsingViewControllerDelegate

- (void)didSelectItem:(TableViewItem*)item {
  ItemType type = static_cast<ItemType>(item.type);
  if (type == ItemTypeSafeBrowsingEnhancedProtection) {
    if ([self shouldItemTypeHaveCheckmark:type]) {
      base::RecordAction(base::UserMetricsAction(
          "SafeBrowsing.Settings.EnhancedProtectionExpandArrowClicked"));
      [self.handler showSafeBrowsingEnhancedProtection];
    } else {
      base::RecordAction(base::UserMetricsAction(
          "SafeBrowsing.Settings.EnhancedProtectionClicked"));
      if (self.openedFromPromoInteraction) {
        base::RecordAction(base::UserMetricsAction(
            "SafeBrowsing.Settings.EnhancedProtectionClickedDueToPromo"));
      }
      [self selectSettingItem:item];
    }
  }

  if (type == ItemTypeSafeBrowsingStandardProtection) {
    if ([self shouldItemTypeHaveCheckmark:type]) {
      base::RecordAction(base::UserMetricsAction(
          "SafeBrowsing.Settings.StandardProtectionExpandArrowClicked"));
      [self.handler showSafeBrowsingStandardProtection];
    } else {
      base::RecordAction(base::UserMetricsAction(
          "SafeBrowsing.Settings.StandardProtectionClicked"));
      [self selectSettingItem:item];
    }
  }

  if (type == ItemTypeSafeBrowsingNoProtection &&
      ![self shouldItemTypeHaveCheckmark:type]) {
    base::RecordAction(base::UserMetricsAction(
        "SafeBrowsing.Settings.DisableSafeBrowsingClicked"));
    [self.handler showSafeBrowsingNoProtectionPopUp:item];
  }
}

- (void)didTapInfoButton:(UIButton*)button onItem:(TableViewItem*)item {
  if ([self shouldEnterprisePopOverDisplay:item]) {
    [self.consumer showEnterprisePopUp:button];
    return;
  }

  // Info button tap logic when not in enterprise mode.
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case ItemTypeSafeBrowsingEnhancedProtection:
      base::RecordAction(base::UserMetricsAction(
          "SafeBrowsing.Settings.EnhancedProtectionExpandArrowClicked"));
      [self.handler showSafeBrowsingEnhancedProtection];
      break;
    case ItemTypeSafeBrowsingStandardProtection:
      base::RecordAction(base::UserMetricsAction(
          "SafeBrowsing.Settings.StandardProtectionExpandArrowClicked"));
      [self.handler showSafeBrowsingStandardProtection];
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

- (void)selectItem {
  for (TableViewItem* item in self.safeBrowsingItems) {
    ItemType type = static_cast<ItemType>(item.type);
    if ([self shouldItemTypeHaveCheckmark:type]) {
      [self.consumer selectItem:item];
      break;
    }
  }
}

#pragma mark - TableViewInfoButtonItemDelegate

- (void)handleTappedInfoButtonForItem:(TableViewItem*)item {
  [self didTapInfoButton:nil onItem:item];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updatePrivacySafeBrowsingSectionAndNotifyConsumer:YES];
}

@end
