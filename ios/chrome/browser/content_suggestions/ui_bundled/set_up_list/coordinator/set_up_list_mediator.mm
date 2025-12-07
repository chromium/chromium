// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/coordinator/set_up_list_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/ios/crb_protocol_observers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/public/set_up_list_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_consumer_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/ui/set_up_list_item_view_data.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_metrics.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/ntp/model/features.h"
#import "ios/chrome/browser/ntp/model/set_up_list.h"
#import "ios/chrome/browser/ntp/model/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"

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

@interface SetUpListMediator () <PrefObserverDelegate,
                                 SceneStateObserver,
                                 SetUpListDelegate,
                                 SetUpListConsumerSource>

@end

@implementation SetUpListMediator {
  SetUpList* _setUpList;
  raw_ptr<PrefService> _localState;
  raw_ptr<PrefService> _prefService;
  // Used by SetUpList to get signed-in status.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrars for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  PrefChangeRegistrar _localStatePrefChangeRegistrar;
  SceneState* _sceneState;
  SetUpListConsumerList* _consumers;
  NSArray<SetUpListConfig*>* _setUpListConfigs;
  // YES if price tracking is enabled for the current user.
  BOOL _priceTrackingEnabled;
}

#pragma mark - Public

- (instancetype)initWithPrefService:(PrefService*)prefService
                    identityManager:(signin::IdentityManager*)identityManager
                         sceneState:(SceneState*)sceneState
              isDefaultSearchEngine:(BOOL)isDefaultSearchEngine
               priceTrackingEnabled:(BOOL)priceTrackingEnabled {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _localState = GetApplicationContext()->GetLocalState();
    _identityManager = identityManager;
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
        ntp_tiles::prefs::kTipsHomeModuleEnabled, &_prefChangeRegistrar);

    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kAppLevelPushNotificationPermissions,
        &_localStatePrefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kFeaturePushNotificationPermissions, &_prefChangeRegistrar);

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

    _setUpList = [SetUpList buildFromPrefs:prefService
                           identityManager:identityManager
                                localState:_localState];
    _setUpList.delegate = self;

    _consumers = [SetUpListConsumerList
        observersWithProtocol:@protocol(SetUpListConsumer)];
    _priceTrackingEnabled = priceTrackingEnabled;
  }
  return self;
}

- (void)disconnect {
  _identityManager = nullptr;
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
    item.priceTrackingEnabled = _priceTrackingEnabled;
    [allItems addObject:item];
  }
  return allItems;
}

- (BOOL)allItemsComplete {
  return [_setUpList allItemsComplete];
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
#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIosCredentialProviderPromoLastActionTaken &&
      CredentialProviderPromoDismissed(_localState)) {
    [self markSetUpListItemPrefComplete:SetUpListItemType::kAutofill];
  } else if (preferenceName == prefs::kIosDefaultBrowserPromoLastAction &&
             DefaultBrowserPromoCompleted()) {
    [self markSetUpListItemPrefComplete:SetUpListItemType::kDefaultBrowser];
  } else if (preferenceName == prefs::kAppLevelPushNotificationPermissions ||
             preferenceName == prefs::kFeaturePushNotificationPermissions) {
    if ([self hasOptedInToNotifications]) {
      [self markSetUpListItemPrefComplete:SetUpListItemType::kNotifications];
    }
  } else if (preferenceName == ntp_tiles::prefs::kTipsHomeModuleEnabled &&
             !_prefService->GetBoolean(
                 ntp_tiles::prefs::kTipsHomeModuleEnabled)) {
    [self hideSetUpList];
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

    item.priceTrackingEnabled = _priceTrackingEnabled;
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

    item.priceTrackingEnabled = _priceTrackingEnabled;
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
  CoreAccountInfo account =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return push_notification_settings::IsMobileNotificationsEnabledForAnyClient(
      account.gaia, _prefService);
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
