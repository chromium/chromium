// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_manager.h"

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remover_observer_bridge.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_item.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_detail_item.h"
#import "ios/chrome/browser/ui/settings/cells/table_view_clear_browsing_data_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_consumer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Maximum number of times to show a notice about other forms of browsing
// history.
const int kMaxTimesHistoryNoticeShown = 1;
// TableViewClearBrowsingDataItem's selectedBackgroundViewBackgroundColorAlpha.
const CGFloat kSelectedBackgroundColorAlpha = 0.05;

// List of flags that have corresponding counters.
const std::vector<BrowsingDataRemoveMask> _browsingDataRemoveFlags = {
    // BrowsingDataRemoveMask::REMOVE_COOKIES not included; we don't have cookie
    // counters yet.
    BrowsingDataRemoveMask::REMOVE_HISTORY,
    BrowsingDataRemoveMask::REMOVE_CACHE,
    BrowsingDataRemoveMask::REMOVE_PASSWORDS,
    BrowsingDataRemoveMask::REMOVE_FORM_DATA,
};

}  // namespace

static NSDictionary* _imageNamesByItemTypes = @{
  [NSNumber numberWithInteger:ItemTypeDataTypeBrowsingHistory] :
      @"clear_browsing_data_history",
  [NSNumber numberWithInteger:ItemTypeDataTypeCookiesSiteData] :
      @"clear_browsing_data_cookies",
  [NSNumber numberWithInteger:ItemTypeDataTypeCache] :
      @"clear_browsing_data_cached_images",
  [NSNumber numberWithInteger:ItemTypeDataTypeSavedPasswords] :
      @"clear_browsing_data_passwords",
  [NSNumber numberWithInteger:ItemTypeDataTypeAutofill] :
      @"clear_browsing_data_autofill",
};

@interface ClearBrowsingDataManager () <BrowsingDataRemoverObserving,
                                        PrefObserverDelegate> {
  // Access to the kDeleteTimePeriod preference.
  IntegerPrefMember _timeRangePref;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Observer for browsing data removal events and associated ScopedObserver
  // used to track registration with BrowsingDataRemover. They both may be
  // null if the new Clear Browser Data UI is disabled.
  std::unique_ptr<BrowsingDataRemoverObserver> observer_;
  std::unique_ptr<
      ScopedObserver<BrowsingDataRemover, BrowsingDataRemoverObserver>>
      scoped_observer_;

  // Corresponds browsing data counters to their masks/flags. Items are inserted
  // as clear data items are constructed. Remains empty if the new Clear Browser
  // Data UI is disabled.
  std::map<BrowsingDataRemoveMask, std::unique_ptr<BrowsingDataCounterWrapper>>
      _countersByMasks;
}

@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// Whether to show alert about other forms of browsing history.
@property(nonatomic, assign)
    BOOL shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
// Whether to show popup other forms of browsing history.
@property(nonatomic, assign)
    BOOL shouldPopupDialogAboutOtherFormsOfBrowsingHistory;
// Whether the mediator is managing a TableViewController or a
// CollectionsViewController.
@property(nonatomic, assign) ClearBrowsingDataListType listType;

// TODO(crbug.com/947456): Prune
// ClearBrowsingDataCollectionViewController-related code when it is dropped.
@property(nonatomic, strong)
    LegacySettingsDetailItem* collectionViewTimeRangeItem;

@property(nonatomic, strong) TableViewDetailIconItem* tableViewTimeRangeItem;

@property(nonatomic, strong)
    BrowsingDataCounterWrapperProducer* counterWrapperProducer;

@end

@implementation ClearBrowsingDataManager
@synthesize browserState = _browserState;
@synthesize consumer = _consumer;
@synthesize linkDelegate = _linkDelegate;
@synthesize shouldShowNoticeAboutOtherFormsOfBrowsingHistory =
    _shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
@synthesize shouldPopupDialogAboutOtherFormsOfBrowsingHistory =
    _shouldPopupDialogAboutOtherFormsOfBrowsingHistory;
@synthesize listType = _listType;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            listType:(ClearBrowsingDataListType)listType {
  return [self initWithBrowserState:browserState
                                listType:listType
                     browsingDataRemover:BrowsingDataRemoverFactory::
                                             GetForBrowserState(browserState)
      browsingDataCounterWrapperProducer:[[BrowsingDataCounterWrapperProducer
                                             alloc] init]];
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              listType:(ClearBrowsingDataListType)listType
                   browsingDataRemover:(BrowsingDataRemover*)remover
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)producer {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _listType = listType;
    _counterWrapperProducer = producer;

    _timeRangePref.Init(browsing_data::prefs::kDeleteTimePeriod,
                        _browserState->GetPrefs());

    if (IsNewClearBrowsingDataUIEnabled()) {
      observer_ = std::make_unique<BrowsingDataRemoverObserverBridge>(self);
      scoped_observer_ = std::make_unique<
          ScopedObserver<BrowsingDataRemover, BrowsingDataRemoverObserver>>(
          observer_.get());
      scoped_observer_->Add(remover);

      _prefChangeRegistrar.Init(_browserState->GetPrefs());
      _prefObserverBridge.reset(new PrefObserverBridge(self));
      _prefObserverBridge->ObserveChangesForPreference(
          browsing_data::prefs::kDeleteTimePeriod, &_prefChangeRegistrar);
    }
  }
  return self;
}

#pragma mark - Public Methods

- (void)loadModel:(ListModel*)model {
  // Time range section.
  // Only implementing new UI for kListTypeCollectionView.
  if (IsNewClearBrowsingDataUIEnabled()) {
    [model addSectionWithIdentifier:SectionIdentifierTimeRange];
    ListItem* timeRangeItem = [self timeRangeItem];
    [model addItem:timeRangeItem
        toSectionWithIdentifier:SectionIdentifierTimeRange];
    if (self.listType == ClearBrowsingDataListType::kListTypeCollectionView) {
      self.collectionViewTimeRangeItem =
          base::mac::ObjCCastStrict<LegacySettingsDetailItem>(timeRangeItem);
    } else {
      DCHECK(self.listType == ClearBrowsingDataListType::kListTypeTableView);
      self.tableViewTimeRangeItem =
          base::mac::ObjCCastStrict<TableViewDetailIconItem>(timeRangeItem);
      self.tableViewTimeRangeItem.useCustomSeparator = YES;
    }
  }

  [self addClearBrowsingDataItemsToModel:model];
  [self addClearDataButtonToModel:model];
  [self addSyncProfileItemsToModel:model];
}

// Add items for types of browsing data to clear.
- (void)addClearBrowsingDataItemsToModel:(ListModel*)model {
  // Data types section.
  [model addSectionWithIdentifier:SectionIdentifierDataTypes];
  ListItem* browsingHistoryItem =
      [self clearDataItemWithType:ItemTypeDataTypeBrowsingHistory
                          titleID:IDS_IOS_CLEAR_BROWSING_HISTORY
                             mask:BrowsingDataRemoveMask::REMOVE_HISTORY
                         prefName:browsing_data::prefs::kDeleteBrowsingHistory];
  [model addItem:browsingHistoryItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  // This data type doesn't currently have an associated counter, but displays
  // an explanatory text instead, when the new UI is enabled.
  ListItem* cookiesSiteDataItem =
      [self clearDataItemWithType:ItemTypeDataTypeCookiesSiteData
                          titleID:IDS_IOS_CLEAR_COOKIES
                             mask:BrowsingDataRemoveMask::REMOVE_SITE_DATA
                         prefName:browsing_data::prefs::kDeleteCookies];
  [model addItem:cookiesSiteDataItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  ListItem* cacheItem =
      [self clearDataItemWithType:ItemTypeDataTypeCache
                          titleID:IDS_IOS_CLEAR_CACHE
                             mask:BrowsingDataRemoveMask::REMOVE_CACHE
                         prefName:browsing_data::prefs::kDeleteCache];
  [model addItem:cacheItem toSectionWithIdentifier:SectionIdentifierDataTypes];

  ListItem* savedPasswordsItem =
      [self clearDataItemWithType:ItemTypeDataTypeSavedPasswords
                          titleID:IDS_IOS_CLEAR_SAVED_PASSWORDS
                             mask:BrowsingDataRemoveMask::REMOVE_PASSWORDS
                         prefName:browsing_data::prefs::kDeletePasswords];
  [model addItem:savedPasswordsItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  ListItem* autofillItem =
      [self clearDataItemWithType:ItemTypeDataTypeAutofill
                          titleID:IDS_IOS_CLEAR_AUTOFILL
                             mask:BrowsingDataRemoveMask::REMOVE_FORM_DATA
                         prefName:browsing_data::prefs::kDeleteFormData];
  [model addItem:autofillItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];
}

- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  if (!result.Finished()) {
    // The counter is still counting.
    return l10n_util::GetNSString(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  base::StringPiece prefName = result.source()->GetPrefName();
  if (prefName != browsing_data::prefs::kDeleteCache) {
    return base::SysUTF16ToNSString(
        browsing_data::GetCounterTextFromResult(&result));
  }

  browsing_data::BrowsingDataCounter::ResultInt cacheSizeBytes =
      static_cast<const browsing_data::BrowsingDataCounter::FinishedResult*>(
          &result)
          ->Value();

  // Three cases: Nonzero result for the entire cache, nonzero result for
  // a subset of cache (i.e. a finite time interval), and almost zero (less
  // than 1 MB). There is no exact information that the cache is empty so that
  // falls into the almost zero case, which is displayed as less than 1 MB.
  // Because of this, the lowest unit that can be used is MB.
  static const int kBytesInAMegabyte = 1 << 20;
  if (cacheSizeBytes >= kBytesInAMegabyte) {
    NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
    formatter.allowedUnits = NSByteCountFormatterUseAll &
                             (~NSByteCountFormatterUseBytes) &
                             (~NSByteCountFormatterUseKB);
    formatter.countStyle = NSByteCountFormatterCountStyleMemory;
    NSString* formattedSize = [formatter stringFromByteCount:cacheSizeBytes];
    return (!IsNewClearBrowsingDataUIEnabled() ||
            _timeRangePref.GetValue() ==
                static_cast<int>(browsing_data::TimePeriod::ALL_TIME))
               ? formattedSize
               : l10n_util::GetNSStringF(
                     IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                     base::SysNSStringToUTF16(formattedSize));
  }

  return l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
}

- (ActionSheetCoordinator*)
    actionSheetCoordinatorWithDataTypesToRemove:
        (BrowsingDataRemoveMask)dataTypeMaskToRemove
                             baseViewController:
                                 (UIViewController*)baseViewController
                                     sourceRect:(CGRect)sourceRect
                                     sourceView:(UIView*)sourceView {
  return [self actionSheetCoordinatorWithDataTypesToRemove:dataTypeMaskToRemove
                                        baseViewController:baseViewController
                                                sourceRect:sourceRect
                                                sourceView:sourceView
                                       sourceBarButtonItem:nil];
}

- (ActionSheetCoordinator*)
    actionSheetCoordinatorWithDataTypesToRemove:
        (BrowsingDataRemoveMask)dataTypeMaskToRemove
                             baseViewController:
                                 (UIViewController*)baseViewController
                            sourceBarButtonItem:
                                (UIBarButtonItem*)sourceBarButtonItem {
  return [self actionSheetCoordinatorWithDataTypesToRemove:dataTypeMaskToRemove
                                        baseViewController:baseViewController
                                                sourceRect:CGRectNull
                                                sourceView:nil
                                       sourceBarButtonItem:sourceBarButtonItem];
}

- (void)addClearDataButtonToModel:(ListModel*)model {
  if (self.listType == ClearBrowsingDataListType::kListTypeTableView &&
      IsNewClearBrowsingDataUIEnabled()) {
    return;
  }
  // Clear Browsing Data button.
  ListItem* clearButtonItem = [self clearButtonItem];
  [model addSectionWithIdentifier:SectionIdentifierClearBrowsingDataButton];
  [model addItem:clearButtonItem
      toSectionWithIdentifier:SectionIdentifierClearBrowsingDataButton];
}

// Add footers about user's account data.
- (void)addSyncProfileItemsToModel:(ListModel*)model {
  // Google Account footer.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(self.browserState);
  if (identityManager->HasPrimaryAccount()) {
    // TODO(crbug.com/650424): Footer items must currently go into a separate
    // section, to work around a drawing bug in MDC.
    [model addSectionWithIdentifier:SectionIdentifierGoogleAccount];
    [model addItem:[self footerForGoogleAccountSectionItem]
        toSectionWithIdentifier:SectionIdentifierGoogleAccount];
  }

  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  if (syncService && syncService->IsSyncFeatureActive()) {
    // TODO(crbug.com/650424): Footer items must currently go into a separate
    // section, to work around a drawing bug in MDC.
    [model addSectionWithIdentifier:SectionIdentifierClearSyncAndSavedSiteData];
    [model addItem:[self footerClearSyncAndSavedSiteDataItem]
        toSectionWithIdentifier:SectionIdentifierClearSyncAndSavedSiteData];
  } else {
    // TODO(crbug.com/650424): Footer items must currently go into a separate
    // section, to work around a drawing bug in MDC.
    [model addSectionWithIdentifier:SectionIdentifierSavedSiteData];
    [model addItem:[self footerSavedSiteDataItem]
        toSectionWithIdentifier:SectionIdentifierSavedSiteData];
  }

  // If not signed in, no need to continue with profile syncing.
  if (!identityManager->HasPrimaryAccount()) {
    return;
  }

  history::WebHistoryService* historyService =
      ios::WebHistoryServiceFactory::GetForBrowserState(_browserState);

  __weak ClearBrowsingDataManager* weakSelf = self;
  browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      syncService, historyService,
      base::BindRepeating(^(bool shouldShowNotice) {
        ClearBrowsingDataManager* strongSelf = weakSelf;
        [strongSelf
            setShouldShowNoticeAboutOtherFormsOfBrowsingHistory:shouldShowNotice
                                                       forModel:model];
      }));

  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      syncService, historyService, GetChannel(),
      base::BindRepeating(^(bool shouldShowPopup) {
        ClearBrowsingDataManager* strongSelf = weakSelf;
        [strongSelf setShouldPopupDialogAboutOtherFormsOfBrowsingHistory:
                        shouldShowPopup];
      }));
}

- (void)restartCounters:(BrowsingDataRemoveMask)mask {
  for (auto flag : _browsingDataRemoveFlags) {
    if (IsRemoveDataMaskSet(mask, flag)) {
      const auto it = _countersByMasks.find(flag);
      if (it != _countersByMasks.end()) {
        it->second->RestartCounter();
      }
    }
  }
}

#pragma mark Items

- (ListItem*)clearButtonItem {
  ListItem* clearButtonItem;
  // Create a SettingsTextItem for CollectionView models and a
  // TableViewTextButtonItem for TableView models.
  if (self.listType == ClearBrowsingDataListType::kListTypeCollectionView) {
    SettingsTextItem* collectionClearButtonItem =
        [[SettingsTextItem alloc] initWithType:ItemTypeClearBrowsingDataButton];
    collectionClearButtonItem.text =
        l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON);
    collectionClearButtonItem.accessibilityTraits |= UIAccessibilityTraitButton;
    collectionClearButtonItem.textColor = [UIColor colorNamed:kRedColor];
    collectionClearButtonItem.accessibilityIdentifier =
        kClearBrowsingDataButtonIdentifier;
    clearButtonItem = collectionClearButtonItem;
  } else {
    TableViewTextButtonItem* tableViewClearButtonItem =
        [[TableViewTextButtonItem alloc]
            initWithType:ItemTypeClearBrowsingDataButton];
    tableViewClearButtonItem.buttonText =
        l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON);
    tableViewClearButtonItem.buttonBackgroundColor =
        [UIColor colorNamed:kRedColor];
    tableViewClearButtonItem.buttonAccessibilityIdentifier =
        kClearBrowsingDataButtonIdentifier;
    clearButtonItem = tableViewClearButtonItem;
  }
  return clearButtonItem;
}

// Creates item of type |itemType| with |mask| of data to be cleared if
// selected, |prefName|, and |titleId| of item.
- (ListItem*)clearDataItemWithType:(ClearBrowsingDataItemType)itemType
                           titleID:(int)titleMessageID
                              mask:(BrowsingDataRemoveMask)mask
                          prefName:(const char*)prefName {
  PrefService* prefs = self.browserState->GetPrefs();
  ListItem* clearDataItem;
  // Create a ClearBrowsingDataItem for a CollectionView model and a
  // TableViewClearBrowsingDataItem for a TableView model.
  if (self.listType == ClearBrowsingDataListType::kListTypeCollectionView) {
    ClearBrowsingDataItem* collectionClearDataItem =
        [[ClearBrowsingDataItem alloc] initWithType:itemType counter:nullptr];
    collectionClearDataItem.text = l10n_util::GetNSString(titleMessageID);
    if (prefs->GetBoolean(prefName)) {
      collectionClearDataItem.accessoryType =
          MDCCollectionViewCellAccessoryCheckmark;
    }
    collectionClearDataItem.dataTypeMask = mask;
    collectionClearDataItem.prefName = prefName;
    collectionClearDataItem.accessibilityIdentifier =
        [self accessibilityIdentifierFromItemType:itemType];
    if (IsNewClearBrowsingDataUIEnabled()) {
      if (itemType == ItemTypeDataTypeCookiesSiteData) {
        // Because there is no counter for cookies, an explanatory text is
        // displayed.
        collectionClearDataItem.detailText =
            l10n_util::GetNSString(IDS_DEL_COOKIES_COUNTER);
      } else {
        __weak ClearBrowsingDataManager* weakSelf = self;
        __weak ClearBrowsingDataItem* weakCollectionClearDataItem =
            collectionClearDataItem;
        BrowsingDataCounterWrapper::UpdateUICallback callback =
            base::BindRepeating(
                ^(const browsing_data::BrowsingDataCounter::Result& result) {
                  weakCollectionClearDataItem.detailText =
                      [weakSelf counterTextFromResult:result];
                  [weakSelf.consumer
                      updateCellsForItem:weakCollectionClearDataItem];
                });
        std::unique_ptr<BrowsingDataCounterWrapper> counter =
            [self.counterWrapperProducer
                createCounterWrapperWithPrefName:prefName
                                    browserState:self.browserState
                                     prefService:prefs
                                updateUiCallback:callback];
        _countersByMasks.emplace(mask, std::move(counter));
      }
    }
    clearDataItem = collectionClearDataItem;
  } else {
    TableViewClearBrowsingDataItem* tableViewClearDataItem =
        [[TableViewClearBrowsingDataItem alloc] initWithType:itemType];
    tableViewClearDataItem.text = l10n_util::GetNSString(titleMessageID);
    tableViewClearDataItem.checked = prefs->GetBoolean(prefName);
    tableViewClearDataItem.accessibilityIdentifier =
        [self accessibilityIdentifierFromItemType:itemType];
    tableViewClearDataItem.dataTypeMask = mask;
    tableViewClearDataItem.prefName = prefName;
    if (IsNewClearBrowsingDataUIEnabled()) {
      tableViewClearDataItem.useCustomSeparator = YES;
      tableViewClearDataItem.checkedBackgroundColor =
          [[UIColor colorNamed:kBlueColor]
              colorWithAlphaComponent:kSelectedBackgroundColorAlpha];
      tableViewClearDataItem.imageName = [_imageNamesByItemTypes
          objectForKey:[NSNumber numberWithInteger:itemType]];
      if (itemType == ItemTypeDataTypeCookiesSiteData) {
        // Because there is no counter for cookies, an explanatory text is
        // displayed.
        tableViewClearDataItem.detailText =
            l10n_util::GetNSString(IDS_DEL_COOKIES_COUNTER);
      } else {
        // Having a placeholder |detailText| helps reduce the observable
        // row-height changes induced by the counter callbacks.
        tableViewClearDataItem.detailText = @"\u00A0";
        __weak ClearBrowsingDataManager* weakSelf = self;
        __weak TableViewClearBrowsingDataItem* weakTableClearDataItem =
            tableViewClearDataItem;
        BrowsingDataCounterWrapper::UpdateUICallback callback =
            base::BindRepeating(
                ^(const browsing_data::BrowsingDataCounter::Result& result) {
                  weakTableClearDataItem.detailText =
                      [weakSelf counterTextFromResult:result];
                  [weakSelf.consumer updateCellsForItem:weakTableClearDataItem];
                });
        std::unique_ptr<BrowsingDataCounterWrapper> counter =
            [self.counterWrapperProducer
                createCounterWrapperWithPrefName:prefName
                                    browserState:self.browserState
                                     prefService:prefs
                                updateUiCallback:callback];
        _countersByMasks.emplace(mask, std::move(counter));
      }
    }
    clearDataItem = tableViewClearDataItem;
  }
  return clearDataItem;
}

- (ListItem*)footerForGoogleAccountSectionItem {
  return _shouldShowNoticeAboutOtherFormsOfBrowsingHistory
             ? [self footerGoogleAccountAndMyActivityItem]
             : [self footerGoogleAccountItem];
}

- (ListItem*)footerGoogleAccountItem {
  ListItem* footerItem;
  // Use CollectionViewFooterItem for CollectionView models and
  // TableViewTextLinkItem for TableView models.
  if (self.listType == ClearBrowsingDataListType::kListTypeCollectionView) {
    CollectionViewFooterItem* collectionFooterItem =
        [[CollectionViewFooterItem alloc]
            initWithType:ItemTypeFooterGoogleAccount];
    collectionFooterItem.cellStyle = CollectionViewCellStyle::kUIKit;
    collectionFooterItem.text =
        l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT);
    UIImage* image = ios::GetChromeBrowserProvider()
                         ->GetBrandedImageProvider()
                         ->GetClearBrowsingDataAccountActivityImage();
    collectionFooterItem.image = image;
    footerItem = collectionFooterItem;
  } else {
    TableViewTextLinkItem* tableViewFooterItem = [[TableViewTextLinkItem alloc]
        initWithType:ItemTypeFooterGoogleAccount];
    tableViewFooterItem.text =
        l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT);
    footerItem = tableViewFooterItem;
  }
  return footerItem;
}

- (ListItem*)footerGoogleAccountAndMyActivityItem {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetBrandedImageProvider()
                       ->GetClearBrowsingDataAccountActivityImage();
  return [self
      footerItemWithType:ItemTypeFooterGoogleAccountAndMyActivity
                 titleID:IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT_AND_HISTORY
                     URL:kClearBrowsingDataMyActivityUrlInFooterURL
                   image:image];
}

- (ListItem*)footerSavedSiteDataItem {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetBrandedImageProvider()
                       ->GetClearBrowsingDataSiteDataImage();
  return [self
      footerItemWithType:ItemTypeFooterSavedSiteData
                 titleID:IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_SAVED_SITE_DATA
                     URL:kClearBrowsingDataLearnMoreURL
                   image:image];
}

- (ListItem*)footerClearSyncAndSavedSiteDataItem {
  UIImage* infoIcon = [ChromeIcon infoIcon];
  UIImage* image = TintImage(infoIcon, [[MDCPalette greyPalette] tint500]);
  return [self
      footerItemWithType:ItemTypeFooterClearSyncAndSavedSiteData
                 titleID:
                     IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_CLEAR_SYNC_AND_SAVED_SITE_DATA
                     URL:kClearBrowsingDataLearnMoreURL
                   image:image];
}

- (ListItem*)footerItemWithType:(ClearBrowsingDataItemType)itemType
                        titleID:(int)titleMessageID
                            URL:(const char[])URL
                          image:(UIImage*)image {
  ListItem* footerItem;
  // Use CollectionViewFooterItem for CollectionView models and
  // TableViewTextLinkItem for TableView models.
  if (self.listType == ClearBrowsingDataListType::kListTypeCollectionView) {
    CollectionViewFooterItem* collectionFooterItem =
        [[CollectionViewFooterItem alloc] initWithType:itemType];
    collectionFooterItem.cellStyle = CollectionViewCellStyle::kUIKit;
    collectionFooterItem.text = l10n_util::GetNSString(titleMessageID);
    collectionFooterItem.linkURL = google_util::AppendGoogleLocaleParam(
        GURL(URL), GetApplicationContext()->GetApplicationLocale());
    collectionFooterItem.linkDelegate = self.linkDelegate;
    collectionFooterItem.image = image;
    footerItem = collectionFooterItem;
  } else {
    TableViewTextLinkItem* tableViewFooterItem =
        [[TableViewTextLinkItem alloc] initWithType:itemType];
    tableViewFooterItem.text = l10n_util::GetNSString(titleMessageID);
    tableViewFooterItem.linkURL = google_util::AppendGoogleLocaleParam(
        GURL(URL), GetApplicationContext()->GetApplicationLocale());
    footerItem = tableViewFooterItem;
  }

  return footerItem;
}

- (ListItem*)timeRangeItem {
  ListItem* timeRangeItem;
  if (self.listType == ClearBrowsingDataListType::kListTypeCollectionView) {
    LegacySettingsDetailItem* collectionTimeRangeItem =
        [[LegacySettingsDetailItem alloc] initWithType:ItemTypeTimeRange];
    collectionTimeRangeItem.text = l10n_util::GetNSString(
        IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE);
    NSString* detailText = [TimeRangeSelectorTableViewController
        timePeriodLabelForPrefs:self.browserState->GetPrefs()];
    DCHECK(detailText);
    collectionTimeRangeItem.detailText = detailText;
    collectionTimeRangeItem.accessoryType =
        MDCCollectionViewCellAccessoryDisclosureIndicator;
    collectionTimeRangeItem.accessibilityTraits |= UIAccessibilityTraitButton;
    timeRangeItem = collectionTimeRangeItem;
  } else {
    DCHECK(self.listType == ClearBrowsingDataListType::kListTypeTableView);
    TableViewDetailIconItem* tableTimeRangeItem =
        [[TableViewDetailIconItem alloc] initWithType:ItemTypeTimeRange];
    tableTimeRangeItem.text = l10n_util::GetNSString(
        IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE);
    NSString* detailText = [TimeRangeSelectorTableViewController
        timePeriodLabelForPrefs:self.browserState->GetPrefs()];
    DCHECK(detailText);

    tableTimeRangeItem.detailText = detailText;
    tableTimeRangeItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
    tableTimeRangeItem.accessibilityTraits |= UIAccessibilityTraitButton;
    timeRangeItem = tableTimeRangeItem;
  }
  return timeRangeItem;
}

- (NSString*)accessibilityIdentifierFromItemType:(NSInteger)itemType {
  switch (itemType) {
    case ItemTypeDataTypeBrowsingHistory:
      return kClearBrowsingHistoryCellAccessibilityIdentifier;
    case ItemTypeDataTypeCookiesSiteData:
      return kClearCookiesCellAccessibilityIdentifier;
    case ItemTypeDataTypeCache:
      return kClearCacheCellAccessibilityIdentifier;
    case ItemTypeDataTypeSavedPasswords:
      return kClearSavedPasswordsCellAccessibilityIdentifier;
    case ItemTypeDataTypeAutofill:
      return kClearAutofillCellAccessibilityIdentifier;
    default: {
      NOTREACHED();
      return nil;
    }
  }
}

#pragma mark - Private Methods

- (void)clearDataForDataTypes:(BrowsingDataRemoveMask)mask {
  DCHECK(mask != BrowsingDataRemoveMask::REMOVE_NOTHING);

  browsing_data::TimePeriod timePeriod =
      IsNewClearBrowsingDataUIEnabled()
          ? static_cast<browsing_data::TimePeriod>(_timeRangePref.GetValue())
          : browsing_data::TimePeriod::ALL_TIME;
  [self.consumer removeBrowsingDataForBrowserState:_browserState
                                        timePeriod:timePeriod
                                        removeMask:mask
                                   completionBlock:nil];

  // Send the "Cleared Browsing Data" event to the feature_engagement::Tracker
  // when the user initiates a clear browsing data action. No event is sent if
  // the browsing data is cleared without the user's input.
  feature_engagement::TrackerFactory::GetForBrowserState(_browserState)
      ->NotifyEvent(feature_engagement::events::kClearedBrowsingData);

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    PrefService* prefs = _browserState->GetPrefs();
    int noticeShownTimes = prefs->GetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

    // When the deletion is complete, we might show an additional dialog with
    // a notice about other forms of browsing history. This is the case if
    const bool showDialog =
        // 1. The dialog is relevant for the user.
        _shouldPopupDialogAboutOtherFormsOfBrowsingHistory &&
        // 2. The notice has been shown less than |kMaxTimesHistoryNoticeShown|.
        noticeShownTimes < kMaxTimesHistoryNoticeShown;
    if (!showDialog) {
      return;
    }
    UMA_HISTOGRAM_BOOLEAN(
        "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing",
        showDialog);

    // Increment the preference.
    prefs->SetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
        noticeShownTimes + 1);
    [self.consumer showBrowsingHistoryRemovedDialog];
  }
}

// Internal helper method which constructs an ActionSheetCoordinator for the two
// |actionSheetCoordinatorWithDataTypesToRemove:...| in the interface.
- (ActionSheetCoordinator*)
    actionSheetCoordinatorWithDataTypesToRemove:
        (BrowsingDataRemoveMask)dataTypeMaskToRemove
                             baseViewController:
                                 (UIViewController*)baseViewController
                                     sourceRect:(CGRect)sourceRect
                                     sourceView:(UIView*)sourceView
                            sourceBarButtonItem:
                                (UIBarButtonItem*)sourceBarButtonItem {
  if (dataTypeMaskToRemove == BrowsingDataRemoveMask::REMOVE_NOTHING) {
    // Nothing to clear (no data types selected).
    return nil;
  }
  __weak ClearBrowsingDataManager* weakSelf = self;

  ActionSheetCoordinator* actionCoordinator;
  if (sourceBarButtonItem) {
    DCHECK(!sourceView);
    actionCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:baseViewController
                             title:l10n_util::GetNSString(
                                       IDS_IOS_CONFIRM_CLEAR_BUTTON_TITLE)
                           message:nil
                     barButtonItem:sourceBarButtonItem];
  } else {
    DCHECK(!sourceBarButtonItem);
    actionCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:baseViewController
                             title:l10n_util::GetNSString(
                                       IDS_IOS_CONFIRM_CLEAR_BUTTON_TITLE)
                           message:nil
                              rect:sourceRect
                              view:sourceView];
  }

  actionCoordinator.popoverArrowDirection =
      UIPopoverArrowDirectionDown | UIPopoverArrowDirectionUp;
  [actionCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)
                action:^{
                  [weakSelf clearDataForDataTypes:dataTypeMaskToRemove];
                }
                 style:UIAlertActionStyleDestructive];
  [actionCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               action:nil
                                style:UIAlertActionStyleCancel];
  return actionCoordinator;
}

#pragma mark Properties

- (void)setShouldShowNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)showNotice
                                                   forModel:(ListModel*)model {
  _shouldShowNoticeAboutOtherFormsOfBrowsingHistory = showNotice;
  // Update the account footer if the model was already loaded.
  if (!model) {
    return;
  }
  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.HistoryNoticeShownInFooterWhenUpdated",
      _shouldShowNoticeAboutOtherFormsOfBrowsingHistory);

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(_browserState);
  if (!identityManager->HasPrimaryAccount()) {
    return;
  }

  ListItem* footerItem = [self footerForGoogleAccountSectionItem];
  // TODO(crbug.com/650424): Simplify with setFooter:inSection: when the bug in
  // MDC is fixed.
  // Remove the footer if there is one in that section.
  if ([model hasSectionForSectionIdentifier:SectionIdentifierGoogleAccount]) {
    if ([model hasItemForItemType:ItemTypeFooterGoogleAccount
                sectionIdentifier:SectionIdentifierGoogleAccount]) {
      [model removeItemWithType:ItemTypeFooterGoogleAccount
          fromSectionWithIdentifier:SectionIdentifierGoogleAccount];
    } else {
      [model removeItemWithType:ItemTypeFooterGoogleAccountAndMyActivity
          fromSectionWithIdentifier:SectionIdentifierGoogleAccount];
    }
  }
  // Add the new footer.
  [model addItem:footerItem
      toSectionWithIdentifier:SectionIdentifierGoogleAccount];
  [self.consumer updateCellsForItem:footerItem];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  DCHECK(preferenceName == browsing_data::prefs::kDeleteTimePeriod);
  NSString* detailText = [TimeRangeSelectorTableViewController
      timePeriodLabelForPrefs:self.browserState->GetPrefs()];
  if (self.listType == ClearBrowsingDataListType::kListTypeCollectionView) {
    self.collectionViewTimeRangeItem.detailText = detailText;
    [self.consumer updateCellsForItem:self.collectionViewTimeRangeItem];
  } else {
    DCHECK(self.listType == ClearBrowsingDataListType::kListTypeTableView);
    self.tableViewTimeRangeItem.detailText = detailText;
    [self.consumer updateCellsForItem:self.tableViewTimeRangeItem];
  }
}

#pragma mark BrowsingDataRemoverObserving

- (void)browsingDataRemover:(BrowsingDataRemover*)remover
    didRemoveBrowsingDataWithMask:(BrowsingDataRemoveMask)mask {
  [self restartCounters:mask];
}

@end
