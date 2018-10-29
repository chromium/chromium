// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_mediator.h"

#include "base/auto_reset.h"
#include "base/mac/foundation_util.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/metrics/metrics_pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_collapsible_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;
using unified_consent::prefs::kUnifiedConsentGiven;

typedef NSArray<CollectionViewItem*>* ItemArray;

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SyncFeedbackSectionIdentifier = kSectionIdentifierEnumZero,
  SyncEverythingSectionIdentifier,
  PersonalizedSectionIdentifier,
  NonPersonalizedSectionIdentifier,
};

// Keys for ListModel to save collapse/expanded prefences, for each section.
NSString* const kGoogleServicesSettingsPersonalizedSectionKey =
    @"GoogleServicesSettingsPersonalizedSection";
NSString* const kGoogleServicesSettingsNonPersonalizedSectionKey =
    @"GoogleServicesSettingsNonPersonalizedSection";

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  // SyncErrorSectionIdentifier,
  SyncErrorItemType = kItemTypeEnumZero,
  // SyncEverythingSectionIdentifier section.
  SyncEverythingItemType,
  // PersonalizedSectionIdentifier section.
  SyncPersonalizationItemType,
  SyncBookmarksItemType,
  SyncHistoryItemType,
  SyncPasswordsItemType,
  SyncOpenTabsItemType,
  SyncAutofillItemType,
  SyncSettingsItemType,
  SyncReadingListItemType,
  AutocompleteWalletItemType,
  SyncActivityAndInteractionsItemType,
  SyncGoogleActivityControlsItemType,
  EncryptionItemType,
  ManageSyncedDataItemType,
  // NonPersonalizedSectionIdentifier section.
  NonPersonalizedServicesItemType,
  AutocompleteSearchesAndURLsItemType,
  PreloadPagesItemType,
  ImproveChromeItemType,
  BetterSearchAndBrowsingItemType,
};

}  // namespace

@interface GoogleServicesSettingsMediator ()<BooleanObserver,
                                             PrefObserverDelegate,
                                             SyncObserverModelBridge> {
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> prefObserverBridge_;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar prefChangeRegistrar_;
  std::unique_ptr<SyncObserverBridge> _syncObserver;
}

// Unified consent service.
@property(nonatomic, assign)
    unified_consent::UnifiedConsentService* unifiedConsentService;
// Returns YES if the user is authenticated.
@property(nonatomic, assign, readonly) BOOL isAuthenticated;
// Returns YES if the user has given his consent to use Google services.
@property(nonatomic, assign, readonly) BOOL isConsentGiven;
// Sync setup service.
@property(nonatomic, assign, readonly) SyncSetupService* syncSetupService;
// Preference value for the autocomplete wallet feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* autocompleteWalletPreference;
// Preference value for the "Autocomplete searches and URLs" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* autocompleteSearchPreference;
// Preference value for the "Preload pages for faster browsing" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* preloadPagesPreference;
// Preference value for the "Preload pages for faster browsing" for Wifi-Only.
// TODO(crbug.com/872101): Needs to create the UI to change from Wifi-Only to
// always
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* preloadPagesWifiOnlyPreference;
// Preference value for the "Help improve Chromium's features" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* sendDataUsagePreference;
// Preference value for the "Help improve Chromium's features" for Wifi-Only.
// TODO(crbug.com/872101): Needs to create the UI to change from Wifi-Only to
// always
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* sendDataUsageWifiOnlyPreference;
// Preference value for the "Make searches and browsing better" feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* anonymizedDataCollectionPreference;

// YES if the switch for |syncEverythingItem| is currently animating from one
// state to another.
@property(nonatomic, assign) BOOL syncEverythingSwitchBeingAnimated;
// YES if at least one switch in the personalized section is currently animating
// from one state to another.
@property(nonatomic, assign) BOOL personalizedSectionBeingAnimated;
// Item to display the sync error.
@property(nonatomic, strong) SettingsImageDetailTextItem* syncErrorItem;
// Item for "Sync Everything" section.
@property(nonatomic, strong, readonly) SyncSwitchItem* syncEverythingItem;
// Collapsible item for the personalized section.
@property(nonatomic, strong, readonly)
    SettingsCollapsibleItem* syncPersonalizationItem;
// All the items for the personalized section.
@property(nonatomic, strong, readonly) ItemArray personalizedItems;
// Item for the autocomplete wallet feature.
@property(nonatomic, strong, readonly) SyncSwitchItem* autocompleteWalletItem;
// Collapsible item for the non-personalized section.
@property(nonatomic, strong, readonly)
    SettingsCollapsibleItem* nonPersonalizedServicesItem;
// All the items for the non-personalized section.
@property(nonatomic, strong, readonly) ItemArray nonPersonalizedItems;

@end

@implementation GoogleServicesSettingsMediator

@synthesize unifiedConsentService = _unifiedConsentService;
@synthesize consumer = _consumer;
@synthesize authService = _authService;
@synthesize syncSetupService = _syncSetupService;
@synthesize autocompleteWalletPreference = _autocompleteWalletPreference;
@synthesize autocompleteSearchPreference = _autocompleteSearchPreference;
@synthesize preloadPagesPreference = _preloadPagesPreference;
@synthesize preloadPagesWifiOnlyPreference = _preloadPagesWifiOnlyPreference;
@synthesize sendDataUsagePreference = _sendDataUsagePreference;
@synthesize sendDataUsageWifiOnlyPreference = _sendDataUsageWifiOnlyPreference;
@synthesize anonymizedDataCollectionPreference =
    _anonymizedDataCollectionPreference;
@synthesize syncEverythingSwitchBeingAnimated =
    _syncEverythingSwitchBeingAnimated;
@synthesize personalizedSectionBeingAnimated =
    _personalizedSectionBeingAnimated;
@synthesize syncErrorItem = _syncErrorItem;
@synthesize syncEverythingItem = _syncEverythingItem;
@synthesize syncPersonalizationItem = _syncPersonalizationItem;
@synthesize personalizedItems = _personalizedItems;
@synthesize autocompleteWalletItem = _autocompleteWalletItem;
@synthesize nonPersonalizedServicesItem = _nonPersonalizedServicesItem;
@synthesize nonPersonalizedItems = _nonPersonalizedItems;

#pragma mark - Load model

- (instancetype)
initWithUserPrefService:(PrefService*)userPrefService
       localPrefService:(PrefService*)localPrefService
            syncService:(browser_sync::ProfileSyncService*)syncService
       syncSetupService:(SyncSetupService*)syncSetupService
  unifiedConsentService:
      (unified_consent::UnifiedConsentService*)unifiedConsentService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    DCHECK(localPrefService);
    DCHECK(syncService);
    DCHECK(syncSetupService);
    DCHECK(unifiedConsentService);
    _syncSetupService = syncSetupService;
    _unifiedConsentService = unifiedConsentService;
    _syncObserver.reset(new SyncObserverBridge(self, syncService));
    prefObserverBridge_ = std::make_unique<PrefObserverBridge>(self);
    prefChangeRegistrar_.Init(userPrefService);
    prefObserverBridge_->ObserveChangesForPreference(kUnifiedConsentGiven,
                                                     &prefChangeRegistrar_);
    _autocompleteWalletPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:autofill::prefs::kAutofillWalletImportEnabled];
    _autocompleteWalletPreference.observer = self;
    _autocompleteSearchPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSearchSuggestEnabled];
    _autocompleteSearchPreference.observer = self;
    _preloadPagesPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kNetworkPredictionEnabled];
    _preloadPagesPreference.observer = self;
    _preloadPagesWifiOnlyPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kNetworkPredictionWifiOnly];
    _sendDataUsagePreference = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:metrics::prefs::kMetricsReportingEnabled];
    _sendDataUsagePreference.observer = self;
    _sendDataUsageWifiOnlyPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:prefs::kMetricsReportingWifiOnly];
    _anonymizedDataCollectionPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:unified_consent::prefs::
                                kUrlKeyedAnonymizedDataCollectionEnabled];
    _anonymizedDataCollectionPreference.observer = self;
  }
  return self;
}

// Loads SyncEverythingSectionIdentifier section.
- (void)loadSyncEverythingSection {
  CollectionViewModel* model = self.consumer.collectionViewModel;
  [model addSectionWithIdentifier:SyncEverythingSectionIdentifier];
  [model addItem:self.syncEverythingItem
      toSectionWithIdentifier:SyncEverythingSectionIdentifier];
  self.syncEverythingItem.on = self.isConsentGiven;
}

// Loads PersonalizedSectionIdentifier section.
- (void)loadPersonalizedSection {
  CollectionViewModel* model = self.consumer.collectionViewModel;
  [model addSectionWithIdentifier:PersonalizedSectionIdentifier];
  [model setSectionIdentifier:PersonalizedSectionIdentifier
                 collapsedKey:kGoogleServicesSettingsPersonalizedSectionKey];
  SettingsCollapsibleItem* syncPersonalizationItem =
      self.syncPersonalizationItem;
  [model addItem:syncPersonalizationItem
      toSectionWithIdentifier:PersonalizedSectionIdentifier];
  BOOL collapsed = self.isAuthenticated ? self.isConsentGiven : YES;
  syncPersonalizationItem.collapsed = collapsed;
  [model setSection:PersonalizedSectionIdentifier collapsed:collapsed];
  for (CollectionViewItem* item in self.personalizedItems) {
    [model addItem:item toSectionWithIdentifier:PersonalizedSectionIdentifier];
  }
  [self updatePersonalizedSection];
}

// Loads NonPersonalizedSectionIdentifier section.
- (void)loadNonPersonalizedSection {
  CollectionViewModel* model = self.consumer.collectionViewModel;
  [model addSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  [model setSectionIdentifier:NonPersonalizedSectionIdentifier
                 collapsedKey:kGoogleServicesSettingsNonPersonalizedSectionKey];
  SettingsCollapsibleItem* nonPersonalizedServicesItem =
      self.nonPersonalizedServicesItem;
  [model addItem:nonPersonalizedServicesItem
      toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  BOOL collapsed = self.isAuthenticated ? self.isConsentGiven : NO;
  nonPersonalizedServicesItem.collapsed = collapsed;
  [model setSection:NonPersonalizedSectionIdentifier collapsed:collapsed];
  for (CollectionViewItem* item in self.nonPersonalizedItems) {
    [model addItem:item
        toSectionWithIdentifier:NonPersonalizedSectionIdentifier];
  }
  [self updateNonPersonalizedSection];
}

#pragma mark - Properties

- (BOOL)isAuthenticated {
  return self.authService->IsAuthenticated();
}

- (BOOL)isConsentGiven {
  return self.unifiedConsentService->IsUnifiedConsentGiven();
}

- (SettingsImageDetailTextItem*)syncErrorItem {
  if (!_syncErrorItem) {
    _syncErrorItem =
        [[SettingsImageDetailTextItem alloc] initWithType:SyncErrorItemType];
    {
      // TODO(crbug.com/889470): Needs asset for the sync error.
      CGSize size = CGSizeMake(40, 40);
      UIGraphicsBeginImageContextWithOptions(size, YES, 0);
      [[UIColor grayColor] setFill];
      UIRectFill(CGRectMake(0, 0, size.width, size.height));
      _syncErrorItem.image = UIGraphicsGetImageFromCurrentImageContext();
      UIGraphicsEndImageContext();
    }
  }
  return _syncErrorItem;
}

- (CollectionViewItem*)syncEverythingItem {
  if (!_syncEverythingItem) {
    _syncEverythingItem = [self
        switchItemWithItemType:SyncEverythingItemType
                  textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_EVERYTHING
                detailStringID:0
                     commandID:
                         GoogleServicesSettingsCommandIDToggleSyncEverything
                      dataType:0];
  }
  return _syncEverythingItem;
}

- (SettingsCollapsibleItem*)syncPersonalizationItem {
  if (!_syncPersonalizationItem) {
    _syncPersonalizationItem = [self
        collapsibleItemWithItemType:SyncPersonalizationItemType
                       textStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_PERSONALIZATION_TEXT
                     detailStringID:
                         IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_PERSONALIZATION_DETAIL];
  }
  return _syncPersonalizationItem;
}

- (ItemArray)personalizedItems {
  if (!_personalizedItems) {
    SyncSwitchItem* syncBookmarksItem = [self
        switchItemWithItemType:SyncBookmarksItemType
                  textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_BOOKMARKS_TEXT
                detailStringID:0
                     commandID:GoogleServicesSettingsCommandIDToggleDataTypeSync
                      dataType:SyncSetupService::kSyncBookmarks];
    SyncSwitchItem* syncHistoryItem = [self
        switchItemWithItemType:SyncHistoryItemType
                  textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_HISTORY_TEXT
                detailStringID:0
                     commandID:GoogleServicesSettingsCommandIDToggleDataTypeSync
                      dataType:SyncSetupService::kSyncOmniboxHistory];
    SyncSwitchItem* syncPasswordsItem = [self
        switchItemWithItemType:SyncPasswordsItemType
                  textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_PASSWORD_TEXT
                detailStringID:0
                     commandID:GoogleServicesSettingsCommandIDToggleDataTypeSync
                      dataType:SyncSetupService::kSyncPasswords];
    SyncSwitchItem* syncOpenTabsItem = [self
        switchItemWithItemType:SyncOpenTabsItemType
                  textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_OPENTABS_TEXT
                detailStringID:0
                     commandID:GoogleServicesSettingsCommandIDToggleDataTypeSync
                      dataType:SyncSetupService::kSyncOpenTabs];
    SyncSwitchItem* syncAutofillItem = [self
        switchItemWithItemType:SyncAutofillItemType
                  textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOFILL_TEXT
                detailStringID:0
                     commandID:GoogleServicesSettingsCommandIDToggleDataTypeSync
                      dataType:SyncSetupService::kSyncAutofill];
    SyncSwitchItem* syncSettingsItem = [self
        switchItemWithItemType:SyncAutofillItemType
                  textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_SETTINGS_TEXT
                detailStringID:0
                     commandID:GoogleServicesSettingsCommandIDToggleDataTypeSync
                      dataType:SyncSetupService::kSyncPreferences];
    SyncSwitchItem* syncReadingListItem = [self
        switchItemWithItemType:SyncReadingListItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_READING_LIST_TEXT
                detailStringID:0
                     commandID:GoogleServicesSettingsCommandIDToggleDataTypeSync
                      dataType:SyncSetupService::kSyncReadingList];
    SyncSwitchItem* syncActivityAndInteractionsItem = [self
        switchItemWithItemType:SyncActivityAndInteractionsItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_DETAIL
                     commandID:GoogleServicesSettingsCommandIDToggleDataTypeSync
                      dataType:SyncSetupService::kSyncUserEvent];
    CollectionViewTextItem* syncGoogleActivityControlsItem = [self
        textItemWithItemType:SyncGoogleActivityControlsItemType
                textStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_GOOGLE_ACTIVITY_CONTROL_TEXT
              detailStringID:
                  IDS_IOS_GOOGLE_SERVICES_SETTINGS_GOOGLE_ACTIVITY_CONTROL_DETAIL
               accessoryType:MDCCollectionViewCellAccessoryDisclosureIndicator
                   commandID:
                       GoogleServicesSettingsCommandIDOpenGoogleActivityControlsDialog];
    CollectionViewTextItem* encryptionItem = [self
        textItemWithItemType:EncryptionItemType
                textStringID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENCRYPTION_TEXT
              detailStringID:0
               accessoryType:MDCCollectionViewCellAccessoryDisclosureIndicator
                   commandID:
                       GoogleServicesSettingsCommandIDOpenEncryptionDialog];
    CollectionViewTextItem* manageSyncedDataItem = [self
        textItemWithItemType:ManageSyncedDataItemType
                textStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_MANAGED_SYNC_DATA_TEXT
              detailStringID:0
               accessoryType:MDCCollectionViewCellAccessoryNone
                   commandID:
                       GoogleServicesSettingsCommandIDOpenManageSyncedDataWebPage];
    _personalizedItems = @[
      syncBookmarksItem, syncHistoryItem, syncPasswordsItem, syncOpenTabsItem,
      syncAutofillItem, syncSettingsItem, syncReadingListItem,
      self.autocompleteWalletItem, syncActivityAndInteractionsItem,
      syncGoogleActivityControlsItem, encryptionItem, manageSyncedDataItem
    ];
  }
  return _personalizedItems;
}

- (SyncSwitchItem*)autocompleteWalletItem {
  if (!_autocompleteWalletItem) {
    _autocompleteWalletItem = [self
        switchItemWithItemType:AutocompleteWalletItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_WALLET
                detailStringID:0
                     commandID:
                         GoogleServicesSettingsCommandIDAutocompleteWalletService
                      dataType:0];
  }
  return _autocompleteWalletItem;
}

- (SettingsCollapsibleItem*)nonPersonalizedServicesItem {
  if (!_nonPersonalizedServicesItem) {
    _nonPersonalizedServicesItem = [self
        collapsibleItemWithItemType:NonPersonalizedServicesItemType
                       textStringID:
                           IDS_IOS_GOOGLE_SERVICES_SETTINGS_NON_PERSONALIZED_SERVICES_TEXT
                     detailStringID:
                         IDS_IOS_GOOGLE_SERVICES_SETTINGS_NON_PERSONALIZED_SERVICES_DETAIL];
  }
  return _nonPersonalizedServicesItem;
}

- (ItemArray)nonPersonalizedItems {
  if (!_nonPersonalizedItems) {
    SyncSwitchItem* autocompleteSearchesAndURLsItem = [self
        switchItemWithItemType:AutocompleteSearchesAndURLsItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL
                     commandID:
                         GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService
                      dataType:0];
    SyncSwitchItem* preloadPagesItem = [self
        switchItemWithItemType:PreloadPagesItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_DETAIL
                     commandID:
                         GoogleServicesSettingsCommandIDTogglePreloadPagesService
                      dataType:0];
    SyncSwitchItem* improveChromeItem = [self
        switchItemWithItemType:ImproveChromeItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL
                     commandID:
                         GoogleServicesSettingsCommandIDToggleImproveChromeService
                      dataType:0];
    SyncSwitchItem* betterSearchAndBrowsingItemType = [self
        switchItemWithItemType:BetterSearchAndBrowsingItemType
                  textStringID:
                      IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
                detailStringID:
                    IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL
                     commandID:
                         GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService
                      dataType:0];
    _nonPersonalizedItems = @[
      autocompleteSearchesAndURLsItem, preloadPagesItem, improveChromeItem,
      betterSearchAndBrowsingItemType
    ];
  }
  return _nonPersonalizedItems;
}

#pragma mark - Private

// Creates a SettingsCollapsibleItem instance.
- (SettingsCollapsibleItem*)collapsibleItemWithItemType:(NSInteger)itemType
                                           textStringID:(int)textStringID
                                         detailStringID:(int)detailStringID {
  SettingsCollapsibleItem* collapsibleItem =
      [[SettingsCollapsibleItem alloc] initWithType:itemType];
  collapsibleItem.text = GetNSString(textStringID);
  collapsibleItem.numberOfTextLines = 0;
  collapsibleItem.detailText = GetNSString(detailStringID);
  collapsibleItem.numberOfDetailTextLines = 0;
  return collapsibleItem;
}

// Creates a SyncSwitchItem instance.
- (SyncSwitchItem*)switchItemWithItemType:(NSInteger)itemType
                             textStringID:(int)textStringID
                           detailStringID:(int)detailStringID
                                commandID:(NSInteger)commandID
                                 dataType:(NSInteger)dataType {
  SyncSwitchItem* switchItem = [[SyncSwitchItem alloc] initWithType:itemType];
  switchItem.text = GetNSString(textStringID);
  if (detailStringID)
    switchItem.detailText = GetNSString(detailStringID);
  switchItem.commandID = commandID;
  switchItem.dataType = dataType;
  return switchItem;
}

// Creates a CollectionViewTextItem instance.
- (CollectionViewTextItem*)
textItemWithItemType:(NSInteger)itemType
        textStringID:(int)textStringID
      detailStringID:(int)detailStringID
       accessoryType:(MDCCollectionViewCellAccessoryType)accessoryType
           commandID:(NSInteger)commandID {
  CollectionViewTextItem* textItem =
      [[CollectionViewTextItem alloc] initWithType:itemType];
  textItem.text = GetNSString(textStringID);
  textItem.accessoryType = accessoryType;
  if (detailStringID)
    textItem.detailText = GetNSString(detailStringID);
  textItem.commandID = commandID;
  return textItem;
}

// Reloads the sync feedback section. If |notifyConsummer| is YES, the consomer
// is notified to add or remove the sync error section.
- (void)updateSyncErrorSectionAndNotifyConsumer:(BOOL)notifyConsummer {
  CollectionViewModel* model = self.consumer.collectionViewModel;
  GoogleServicesSettingsCommandID commandID =
      GoogleServicesSettingsCommandIDNoOp;
  if (self.isAuthenticated) {
    switch (self.syncSetupService->GetSyncServiceState()) {
      case SyncSetupService::kSyncServiceUnrecoverableError:
        commandID = GoogleServicesSettingsCommandIDRestartAuthenticationFlow;
        break;
      case SyncSetupService::kSyncServiceSignInNeedsUpdate:
        commandID = GoogleServicesSettingsReauthDialogAsSyncIsInAuthError;
        break;
      case SyncSetupService::kSyncServiceNeedsPassphrase:
        commandID = GoogleServicesSettingsCommandIDShowPassphraseDialog;
        break;
      case SyncSetupService::kNoSyncServiceError:
      case SyncSetupService::kSyncServiceCouldNotConnect:
      case SyncSetupService::kSyncServiceServiceUnavailable:
        break;
    }
  }
  if (commandID == GoogleServicesSettingsCommandIDNoOp) {
    // No action to do, therefore the sync error section should not be visibled.
    if ([model hasSectionForSectionIdentifier:SyncFeedbackSectionIdentifier]) {
      // Remove the sync error item if it exists.
      NSUInteger sectionIndex =
          [model sectionForSectionIdentifier:SyncFeedbackSectionIdentifier];
      [model removeSectionWithIdentifier:SyncFeedbackSectionIdentifier];
      if (notifyConsummer) {
        NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
        [self.consumer deleteSections:indexSet];
      }
    }
    return;
  }
  // Add the sync error item and its section (if it doesn't already exist) and
  // reload them.
  BOOL sectionAdded = NO;
  if (![model hasSectionForSectionIdentifier:SyncFeedbackSectionIdentifier]) {
    // Adding the sync error item and its section.
    [model insertSectionWithIdentifier:SyncFeedbackSectionIdentifier atIndex:0];
    [model addItem:self.syncErrorItem
        toSectionWithIdentifier:SyncFeedbackSectionIdentifier];
    sectionAdded = YES;
  }
  NSUInteger sectionIndex =
      [model sectionForSectionIdentifier:SyncFeedbackSectionIdentifier];
  self.syncErrorItem.text = l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_TITLE);
  self.syncErrorItem.detailText =
      GetSyncErrorDescriptionForSyncSetupService(self.syncSetupService);
  self.syncErrorItem.commandID = commandID;
  if (notifyConsummer) {
    if (sectionAdded) {
      NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
      [self.consumer insertSections:indexSet];
    } else {
      [self.consumer reloadItem:self.syncErrorItem];
    }
  }
}

// Updates the personalized section according to the user consent.
- (void)updatePersonalizedSection {
  BOOL enabled = self.isAuthenticated && !self.isConsentGiven;
  [self updateSectionWithCollapsibleItem:self.syncPersonalizationItem
                                   items:self.personalizedItems
                  collapsibleItemEnabled:enabled
                       switchItemEnabled:enabled
                         textItemEnabled:self.isAuthenticated];
  syncer::ModelType autofillModelType =
      _syncSetupService->GetModelType(SyncSetupService::kSyncAutofill);
  BOOL isAutofillOn = _syncSetupService->IsDataTypePreferred(autofillModelType);
  self.autocompleteWalletItem.enabled = enabled && isAutofillOn;
  if (!isAutofillOn) {
    // Autocomplete wallet item should be disabled when autofill is off.
    self.autocompleteWalletItem.on = false;
  }
}

// Updates the non-personalized section according to the user consent.
- (void)updateNonPersonalizedSection {
  BOOL enabled = !self.isAuthenticated || !self.isConsentGiven;
  [self updateSectionWithCollapsibleItem:self.nonPersonalizedServicesItem
                                   items:self.nonPersonalizedItems
                  collapsibleItemEnabled:enabled
                       switchItemEnabled:enabled
                         textItemEnabled:enabled];
}

// Updates |collapsibleItem| and |items| using |collapsibleItemEnabled|,
// |switchItemEnabled| and |textItemEnabled|.
- (void)updateSectionWithCollapsibleItem:
            (SettingsCollapsibleItem*)collapsibleItem
                                   items:(ItemArray)items
                  collapsibleItemEnabled:(BOOL)collapsibleItemEnabled
                       switchItemEnabled:(BOOL)switchItemEnabled
                         textItemEnabled:(BOOL)textItemEnabled {
  UIColor* textColor =
      collapsibleItemEnabled ? nil : [[MDCPalette greyPalette] tint500];
  collapsibleItem.textColor = textColor;
  for (CollectionViewItem* item in items) {
    if ([item isKindOfClass:[SyncSwitchItem class]]) {
      SyncSwitchItem* switchItem = base::mac::ObjCCast<SyncSwitchItem>(item);
      switch (switchItem.commandID) {
        case GoogleServicesSettingsCommandIDToggleDataTypeSync: {
          SyncSetupService::SyncableDatatype dataType =
              static_cast<SyncSetupService::SyncableDatatype>(
                  switchItem.dataType);
          syncer::ModelType modelType =
              self.syncSetupService->GetModelType(dataType);
          switchItem.on = self.syncSetupService->IsDataTypePreferred(modelType);
          break;
        }
        case GoogleServicesSettingsCommandIDAutocompleteWalletService:
          switchItem.on = self.autocompleteWalletPreference.value;
          break;
        case GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService:
          switchItem.on = self.autocompleteSearchPreference.value;
          break;
        case GoogleServicesSettingsCommandIDTogglePreloadPagesService:
          switchItem.on = self.preloadPagesPreference.value;
          break;
        case GoogleServicesSettingsCommandIDToggleImproveChromeService:
          switchItem.on = self.sendDataUsagePreference.value;
          break;
        case GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService:
          switchItem.on = self.anonymizedDataCollectionPreference.value;
          break;
        case GoogleServicesSettingsCommandIDOpenGoogleActivityControlsDialog:
        case GoogleServicesSettingsCommandIDOpenEncryptionDialog:
        case GoogleServicesSettingsCommandIDOpenManageSyncedDataWebPage:
          NOTREACHED();
          break;
      }
      switchItem.enabled = switchItemEnabled;
    } else if ([item isKindOfClass:[CollectionViewTextItem class]]) {
      CollectionViewTextItem* textItem =
          base::mac::ObjCCast<CollectionViewTextItem>(item);
      textItem.enabled = textItemEnabled;
    } else {
      NOTREACHED();
    }
  }
}

#pragma mark - GoogleServicesSettingsViewControllerModelDelegate

- (void)googleServicesSettingsViewControllerLoadModel:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.consumer, controller);
  self.consumer.collectionViewModel.collapsableMode =
      ListModelCollapsableModeFirstCell;
  if (self.isAuthenticated)
    [self loadSyncEverythingSection];
  [self loadPersonalizedSection];
  [self loadNonPersonalizedSection];
  [self updateSyncErrorSectionAndNotifyConsumer:NO];
}

#pragma mark - GoogleServicesSettingsServiceDelegate

- (void)toggleSyncEverythingWithValue:(BOOL)value {
  if (value == self.isConsentGiven)
    return;
  // Mark the switch has being animated to avoid being reloaded.
  base::AutoReset<BOOL> autoReset(&_syncEverythingSwitchBeingAnimated, YES);
  self.unifiedConsentService->SetUnifiedConsentGiven(value);
}

- (void)toggleSyncDataSync:(NSInteger)dataTypeInt withValue:(BOOL)value {
  base::AutoReset<BOOL> autoReset(&_personalizedSectionBeingAnimated, YES);
  SyncSetupService::SyncableDatatype dataType =
      static_cast<SyncSetupService::SyncableDatatype>(dataTypeInt);
  syncer::ModelType modelType = self.syncSetupService->GetModelType(dataType);
  self.syncSetupService->SetDataTypeEnabled(modelType, value);
}

- (void)toggleAutocompleteWalletServiceWithValue:(BOOL)value {
  self.autocompleteWalletPreference.value = value;
}

- (void)toggleAutocompleteSearchesServiceWithValue:(BOOL)value {
  self.autocompleteSearchPreference.value = value;
}

- (void)togglePreloadPagesServiceWithValue:(BOOL)value {
  self.preloadPagesPreference.value = value;
  if (value) {
    // Should be wifi only, until https://crbug.com/872101 is fixed.
    self.preloadPagesWifiOnlyPreference.value = YES;
  }
}

- (void)toggleImproveChromeServiceWithValue:(BOOL)value {
  self.sendDataUsagePreference.value = value;
  if (value) {
    // Should be wifi only, until https://crbug.com/872101 is fixed.
    self.sendDataUsageWifiOnlyPreference.value = YES;
  }
}

- (void)toggleBetterSearchAndBrowsingServiceWithValue:(BOOL)value {
  self.anonymizedDataCollectionPreference.value = value;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  DCHECK_EQ(kUnifiedConsentGiven, preferenceName);
  self.syncEverythingItem.on = self.isConsentGiven;
  [self updatePersonalizedSection];
  [self updateNonPersonalizedSection];
  CollectionViewModel* model = self.consumer.collectionViewModel;
  if (!self.isConsentGiven) {
    // If the consent is removed, both collapsible sections should be expanded.
    [model setSection:PersonalizedSectionIdentifier collapsed:NO];
    [self syncPersonalizationItem].collapsed = NO;
    [model setSection:NonPersonalizedSectionIdentifier collapsed:NO];
    [self nonPersonalizedServicesItem].collapsed = NO;
  }
  // Reload sections.
  NSMutableIndexSet* sectionIndexToReload = [NSMutableIndexSet indexSet];
  if (!self.syncEverythingSwitchBeingAnimated) {
    // The sync everything section can be reloaded only if the switch for
    // syncEverythingItem is not currently animated. Otherwise the animation
    // would be stopped before the end.
    [sectionIndexToReload addIndex:[model sectionForSectionIdentifier:
                                              SyncEverythingSectionIdentifier]];
  }
  if (!self.personalizedSectionBeingAnimated) {
    // The sync everything section can be reloaded only if none of the switches
    // in the personalized section are not currently animated. Otherwise the
    // animation would be stopped before the end.
    [sectionIndexToReload addIndex:[model sectionForSectionIdentifier:
                                              PersonalizedSectionIdentifier]];
  }
  [sectionIndexToReload addIndex:[model sectionForSectionIdentifier:
                                            NonPersonalizedSectionIdentifier]];
  [self.consumer reloadSections:sectionIndexToReload];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updatePersonalizedSection];
  if (!self.personalizedSectionBeingAnimated) {
    CollectionViewModel* model = self.consumer.collectionViewModel;
    NSMutableIndexSet* sectionIndexToReload = [NSMutableIndexSet indexSet];
    [sectionIndexToReload addIndex:[model sectionForSectionIdentifier:
                                              PersonalizedSectionIdentifier]];
    [self.consumer reloadSections:sectionIndexToReload];
  } else {
    // Needs to reload only the autocomplete wallet item (which is part of the
    // personalized section), if the autofill feature changed state.
    [self.consumer reloadItem:self.autocompleteWalletItem];
  }
  [self updateSyncErrorSectionAndNotifyConsumer:YES];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateNonPersonalizedSection];
  CollectionViewModel* model = self.consumer.collectionViewModel;
  NSUInteger index =
      [model sectionForSectionIdentifier:NonPersonalizedSectionIdentifier];
  NSIndexSet* sectionIndexToReload = [NSIndexSet indexSetWithIndex:index];
  [self.consumer reloadSections:sectionIndexToReload];
}

@end
