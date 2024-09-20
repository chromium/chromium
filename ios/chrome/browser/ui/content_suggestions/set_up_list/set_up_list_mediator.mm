// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/ios/crb_protocol_observers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_metrics.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/ntp/model/set_up_list.h"
#import "ios/chrome/browser/ntp/model/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_config.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_consumer_source.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"

using credential_provider_promo::IOSCredentialProviderPromoAction;

namespace {

// Checks the last action the user took on the Credential Provider Promo to
// determine if it was dismissed.
bool CredentialProviderPromoDismissed(PrefService* local_state) {
  IOSCredentialProviderPromoAction last_action =
      static_cast<IOSCredentialProviderPromoAction>(local_state->GetInteger(
          prefs::kIosCredentialProviderPromoLastActionTaken));
  return last_action == IOSCredentialProviderPromoAction::kNo;
}

// Returns true if a Default Browser Promo was completed, outside of SetUpList.
// This includes the FRE.
bool DefaultBrowserPromoCompleted() {
  std::optional<IOSDefaultBrowserPromoAction> action =
      DefaultBrowserPromoLastAction();
  if (!action.has_value()) {
    return false;
  }

  switch (action.value()) {
    case IOSDefaultBrowserPromoAction::kActionButton:
    case IOSDefaultBrowserPromoAction::kCancel:
      return true;
    case IOSDefaultBrowserPromoAction::kRemindMeLater:
    case IOSDefaultBrowserPromoAction::kDismiss:
      return false;
  }
}

}  // namespace

#pragma mark - SetUpListConsumerList

@interface SetUpListConsumerList : CRBProtocolObservers <SetUpListConsumer>
@end

@implementation SetUpListConsumerList
@end

@interface SetUpListMediator () <AuthenticationServiceObserving,
                                 IdentityManagerObserverBridgeDelegate,
                                 PrefObserverDelegate,
                                 SceneStateObserver,
                                 SetUpListDelegate,
                                 SetUpListConsumerSource,
                                 SyncObserverModelBridge>

@end

@implementation SetUpListMediator {
  SetUpList* _setUpList;
  raw_ptr<PrefService> _localState;
  raw_ptr<PrefService> _prefService;
  // Used by SetUpList to get the sync status.
  raw_ptr<syncer::SyncService> _syncService;
  // Observer for sync service status changes.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // Observes changes to signed-in status.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // Used by SetUpList to get signed-in status.
  raw_ptr<AuthenticationService> _authenticationService;
  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrars for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  PrefChangeRegistrar _localStatePrefChangeRegistrar;
  SceneState* _sceneState;
  SetUpListConsumerList* _consumers;
  NSArray<SetUpListConfig*>* _setUpListConfigs;
  // Components for retrieving user segmentation information from the
  // Segmentation Platform.
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      _segmentationService;
  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;
  // User segment retrieved by the Segmentation Platform.
  segmentation_platform::DefaultBrowserUserSegment _userSegment;
}

#pragma mark - Public

- (instancetype)initWithPrefService:(PrefService*)prefService
                        syncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
              authenticationService:(AuthenticationService*)authService
                         sceneState:(SceneState*)sceneState
              isDefaultSearchEngine:(BOOL)isDefaultSearchEngine
                segmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
     deviceSwitcherResultDispatcher:
         (segmentation_platform::DeviceSwitcherResultDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _localState = GetApplicationContext()->GetLocalState();
    _syncService = syncService;
    _syncObserverBridge =
        std::make_unique<SyncObserverBridge>(self, syncService);
    _identityObserverBridge =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _authenticationService = authService;
    _authServiceObserverBridge =
        std::make_unique<AuthenticationServiceObserverBridge>(authService,
                                                              self);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _localStatePrefChangeRegistrar.Init(_localState);
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosCredentialProviderPromoLastActionTaken,
        &_localStatePrefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosDefaultBrowserPromoLastAction,
        &_localStatePrefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kDisabled, &_localStatePrefChangeRegistrar);

    if (IsHomeCustomizationEnabled()) {
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kHomeCustomizationMagicStackSetUpListEnabled,
          &_prefChangeRegistrar);
    }

    if (IsIOSTipsNotificationsEnabled()) {
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kAppLevelPushNotificationPermissions,
          &_localStatePrefChangeRegistrar);
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kFeaturePushNotificationPermissions, &_prefChangeRegistrar);
    }
    if (CredentialProviderPromoDismissed(_localState)) {
      set_up_list_prefs::MarkItemComplete(_localState,
                                          SetUpListItemType::kAutofill);
    } else {
      [self checkIfCPEEnabled];
    }

    if (DefaultBrowserPromoCompleted()) {
      set_up_list_prefs::MarkItemComplete(_localState,
                                          SetUpListItemType::kDefaultBrowser);
    }

    _sceneState = sceneState;
    [_sceneState addObserver:self];

    if (IsSegmentedDefaultBrowserPromoEnabled()) {
      _segmentationService = segmentationService;
      _deviceSwitcherResultDispatcher = dispatcher;
    }
    BOOL isContentNotificationEnabled =
        IsContentNotificationExperimentEnabled() &&
        IsContentNotificationSetUpListEnabled(
            identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin),
            isDefaultSearchEngine, prefService);

    _setUpList = [SetUpList buildFromPrefs:prefService
                                localState:_localState
                               syncService:syncService
                     authenticationService:authService
                contentNotificationEnabled:isContentNotificationEnabled];
    _setUpList.delegate = self;

    _consumers = [SetUpListConsumerList
        observersWithProtocol:@protocol(SetUpListConsumer)];
  }
  return self;
}

- (void)disconnect {
  _segmentationService = nullptr;
  _deviceSwitcherResultDispatcher = nullptr;
  _authenticationService = nullptr;
  _authServiceObserverBridge.reset();
  _syncObserverBridge.reset();
  _identityObserverBridge.reset();
  if (_prefObserverBridge) {
    _localStatePrefChangeRegistrar.RemoveAll();
    _prefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
  [_setUpList disconnect];
  _setUpList = nil;
  [_sceneState removeObserver:self];
  _localState = nullptr;
  _prefService = nullptr;
}

- (void)addConsumer:(id<SetUpListConsumer>)consumer {
  [_consumers addObserver:consumer];
}

- (void)removeConsumer:(id<SetUpListConsumer>)consumer {
  [_consumers removeObserver:consumer];
}

- (NSArray<SetUpListItemViewData*>*)allItems {
  NSMutableArray<SetUpListItemViewData*>* allItems =
      [[NSMutableArray alloc] init];
  for (SetUpListItem* model in _setUpList.allItems) {
    SetUpListItemViewData* item =
        [[SetUpListItemViewData alloc] initWithType:model.type
                                           complete:model.complete];
    if (IsSegmentedDefaultBrowserPromoEnabled()) {
      [item setUserSegment:_userSegment];
    }
    [allItems addObject:item];
  }
  return allItems;
}

- (BOOL)allItemsComplete {
  return [_setUpList allItemsComplete];
}

- (void)disableModule {
  set_up_list_prefs::DisableSetUpList(
      IsHomeCustomizationEnabled() ? _prefService : _localState);
}

- (BOOL)shouldShowSetUpList {
  if (!set_up_list_utils::IsSetUpListActive(_localState, _prefService)) {
    return NO;
  }

  if ([self setUpListItems].count == 0) {
    return NO;
  }

  return YES;
}

- (NSArray<SetUpListConfig*>*)setUpListConfigs {
  if (!_setUpListConfigs) {
    if ([self allItemsComplete]) {
      SetUpListConfig* config = [[SetUpListConfig alloc] init];
      config.setUpListConsumerSource = self;
      config.commandHandler = self.commandHandler;
      config.setUpListItems = @[ [self allSetItem] ];
      _setUpListConfigs = @[ config ];
    } else {
      NSArray<SetUpListItemViewData*>* items = [self setUpListItems];
      if (set_up_list_utils::ShouldShowCompactedSetUpListModule()) {
        SetUpListConfig* config = [[SetUpListConfig alloc] init];
        config.shouldShowCompactModule = YES;
        config.shouldShowSeeMore = YES;
        config.setUpListConsumerSource = self;
        config.commandHandler = self.commandHandler;

        if ([items count] > 2) {
          items = [items subarrayWithRange:NSMakeRange(0, 2)];
        }
        for (SetUpListItemViewData* data in items) {
          data.compactLayout = YES;
          data.heroCellMagicStackLayout = NO;
        }
        config.setUpListItems = items;
        _setUpListConfigs = @[ config ];
      } else {
        // Iterate through all items and create config for each hero module.
        NSMutableArray<SetUpListConfig*>* configs = [NSMutableArray array];
        for (SetUpListItemViewData* data in items) {
          data.compactLayout = NO;
          data.heroCellMagicStackLayout = YES;
          SetUpListConfig* config = [[SetUpListConfig alloc] init];
          config.setUpListConsumerSource = self;
          config.commandHandler = self.commandHandler;

          config.setUpListItems = @[ data ];
          [configs addObject:config];
        }
        _setUpListConfigs = configs;
      }
    }

    // Record "ItemDisplayed" histogram for each item.
    for (SetUpListConfig* config in _setUpListConfigs) {
      for (SetUpListItemViewData* item in config.setUpListItems) {
        [self.contentSuggestionsMetricsRecorder
            recordSetUpListItemShown:item.type];
      }
    }
    [self.contentSuggestionsMetricsRecorder recordSetUpListShown];
  }
  return _setUpListConfigs;
}

- (void)retrieveUserSegment {
  CHECK(_segmentationService);
  CHECK(_deviceSwitcherResultDispatcher);
  segmentation_platform::PredictionOptions options =
      segmentation_platform::PredictionOptions::ForCached();

  segmentation_platform::ClassificationResult deviceSwitcherResult =
      _deviceSwitcherResultDispatcher->GetCachedClassificationResult();

  __weak __typeof(self) weakSelf = self;
  auto classificationResultCallback = base::BindOnce(
      [](__typeof(self) strongSelf,
         segmentation_platform::ClassificationResult deviceSwitcherResult,

         const segmentation_platform::ClassificationResult& shopperResult) {
        [strongSelf didReceiveShopperSegmentationResult:shopperResult
                                   deviceSwitcherResult:deviceSwitcherResult];
      },
      weakSelf, deviceSwitcherResult);
  _segmentationService->GetClassificationResult(
      segmentation_platform::kShoppingUserSegmentationKey, options, nullptr,
      std::move(classificationResultCallback));
}

#pragma mark - SetUpListDelegate

- (void)setUpListItemDidComplete:(SetUpListItem*)item
               allItemsCompleted:(BOOL)completed {
  // Can resend signal to mediator from Set Up List after SetUpListItemView
  // completes animation
  ProceduralBlock completion = nil;
  if (completed) {
    if (![self configsContainItem:item.type]) {
      // If the last item to complete is not in the displayed list, immediately
      // swap to "All Set" rather than wait for any animations to finish.
      [self replaceSetUpListWithAllSet];
    } else {
      // Wait until item completion animation is finished to swap in "All Set".
      __weak __typeof(self) weakSelf = self;
      completion = ^{
        [weakSelf replaceSetUpListWithAllSet];
      };
    }
  }
  [_consumers setUpListItemDidComplete:item
                     allItemsCompleted:completed
                            completion:completion];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // User has signed in, mark SetUpList item complete. Delayed to allow
      // Signin UI flow to be fully dismissed before starting SetUpList
      // completion animation.
      __weak __typeof(self) weakSelf = self;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(^{
            [weakSelf
                markSetUpListItemPrefComplete:SetUpListItemType::kSignInSync];
          }),
          base::Seconds(0.5));
    } break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIosCredentialProviderPromoLastActionTaken &&
      CredentialProviderPromoDismissed(_localState)) {
    [self markSetUpListItemPrefComplete:SetUpListItemType::kAutofill];
  } else if (preferenceName == prefs::kIosDefaultBrowserPromoLastAction &&
             DefaultBrowserPromoCompleted()) {
    [self markSetUpListItemPrefComplete:SetUpListItemType::kDefaultBrowser];
  } else if (preferenceName == set_up_list_prefs::kDisabled &&
             set_up_list_prefs::IsSetUpListDisabled(_localState)) {
    [self hideSetUpList];
  } else if (preferenceName == prefs::kAppLevelPushNotificationPermissions ||
             preferenceName == prefs::kFeaturePushNotificationPermissions) {
    CHECK(IsIOSTipsNotificationsEnabled());
    if ([self hasOptedInToNotifications]) {
      [self markSetUpListItemPrefComplete:SetUpListItemType::kNotifications];
    }
  } else if (preferenceName ==
                 prefs::kHomeCustomizationMagicStackSetUpListEnabled &&
             !_prefService->GetBoolean(
                 prefs::kHomeCustomizationMagicStackSetUpListEnabled)) {
    CHECK(IsHomeCustomizationEnabled());
    [self hideSetUpList];
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_setUpList) {
    if (_syncService->HasDisableReason(
            syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
        HasManagedSyncDataType(_syncService)) {
      // Sync is now disabled, so mark the SetUpList item complete so that it
      // cannot be used again.
      [self markSetUpListItemPrefComplete:SetUpListItemType::kSignInSync];
    }
  }
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  if (_setUpList) {
    switch (_authenticationService->GetServiceStatus()) {
      case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
      case AuthenticationService::ServiceStatus::SigninAllowed:
        break;
      case AuthenticationService::ServiceStatus::SigninDisabledByUser:
      case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
      case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
        // Signin is now disabled, so mark the SetUpList item complete so that
        // it cannot be used again.
        [self markSetUpListItemPrefComplete:SetUpListItemType::kSignInSync];
    }
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    if (_setUpList) {
      [self checkIfCPEEnabled];
    }
  }
}

#pragma mark - Private

- (NSArray<SetUpListItemViewData*>*)setUpListItems {
  NSMutableArray<SetUpListItemViewData*>* items = [[NSMutableArray alloc] init];

  // Add items that are not complete yet.
  for (SetUpListItem* model in _setUpList.items) {
    if (model.complete) {
      continue;
    }
    SetUpListItemViewData* item =
        [[SetUpListItemViewData alloc] initWithType:model.type
                                           complete:model.complete];

    if (IsSegmentedDefaultBrowserPromoEnabled()) {
      [item setUserSegment:_userSegment];
    }
    [items addObject:item];
  }

  // Add items that are complete to the end.
  for (SetUpListItem* model in _setUpList.items) {
    if (!model.complete) {
      continue;
    }
    SetUpListItemViewData* item =
        [[SetUpListItemViewData alloc] initWithType:model.type
                                           complete:model.complete];

    if (IsSegmentedDefaultBrowserPromoEnabled()) {
      [item setUserSegment:_userSegment];
    }
    [items addObject:item];
  }
  return items;
}

// Sets the pref for a SetUpList item to indicate it is complete.
- (void)markSetUpListItemPrefComplete:(SetUpListItemType)type {
  // Exit early if this is called after `disconnect` which clears _localState.
  // Item states will be reevaluated the next time this mediator is loaded.
  if (!_localState) {
    return;
  }
  set_up_list_prefs::MarkItemComplete(_localState, type);
}

// Returns an item for the "All Set" Set Up List state.
- (SetUpListItemViewData*)allSetItem {
  SetUpListItemViewData* allSetItem =
      [[SetUpListItemViewData alloc] initWithType:SetUpListItemType::kAllSet
                                         complete:NO];
  allSetItem.compactLayout = NO;
  allSetItem.heroCellMagicStackLayout = YES;
  return allSetItem;
}

// Hides the Set Up List with an animation.
- (void)hideSetUpList {
  [self.audience removeSetUpList];
}

// Checks if the CPE is enabled and marks the SetUpList Autofill item complete
// if it is.
- (void)checkIfCPEEnabled {
  __weak __typeof(self) weakSelf = self;
  scoped_refptr<base::SequencedTaskRunner> runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  [ASCredentialIdentityStore.sharedStore
      getCredentialIdentityStoreStateWithCompletion:^(
          ASCredentialIdentityStoreState* state) {
        if (state.isEnabled) {
          // The completion handler sent to ASCredentialIdentityStore is
          // executed on a background thread. Putting it back onto the main
          // thread to update local state prefs.
          runner->PostTask(
              FROM_HERE, base::BindOnce(^{
                [weakSelf
                    markSetUpListItemPrefComplete:SetUpListItemType::kAutofill];
              }));
        }
      }];
}

- (BOOL)hasOptedInToNotifications {
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  return push_notification_settings::IsMobileNotificationsEnabledForAnyClient(
      base::SysNSStringToUTF8(identity.gaiaID), _prefService);
}

// Sets user's highest priority segment retrieved from the Segmentation
// Platform.
- (void)didReceiveShopperSegmentationResult:
            (const segmentation_platform::ClassificationResult&)shopperResult
                       deviceSwitcherResult:
                           (const segmentation_platform::ClassificationResult&)
                               deviceSwitcherResult {
  _userSegment =
      GetDefaultBrowserUserSegment(&deviceSwitcherResult, &shopperResult);
}

// Returns YES if the current configs contains an item with the given `type`.
- (BOOL)configsContainItem:(SetUpListItemType)type {
  for (SetUpListConfig* config in [self setUpListConfigs]) {
    for (SetUpListItem* item in config.setUpListItems) {
      if (item.type == type) {
        return YES;
      }
    }
  }
  return NO;
}

// Swaps out all SetUpList items with the "All Set" item.
- (void)replaceSetUpListWithAllSet {
  SetUpListConfig* config = [[SetUpListConfig alloc] init];
  config.setUpListItems = @[ [self allSetItem] ];
  [self.audience replaceSetUpListWithAllSet:config];
}

@end
