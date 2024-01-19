// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "base/ios/crb_protocol_observers.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/ntp/model/set_up_list.h"
#import "ios/chrome/browser/ntp/model/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_metrics.h"

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

}  // namespace

#pragma mark - SetUpListMediatorObserverList

@interface SetUpListMediatorObserverList
    : CRBProtocolObservers <SetUpListMediatorObserver>
@end

@implementation SetUpListMediatorObserverList
@end

@interface SetUpListMediator () <AuthenticationServiceObserving,
                                 IdentityManagerObserverBridgeDelegate,
                                 PrefObserverDelegate,
                                 SceneStateObserver,
                                 SetUpListDelegate,
                                 SyncObserverModelBridge>

@end

@implementation SetUpListMediator {
  SetUpList* _setUpList;
  PrefService* _localState;
  // Used by SetUpList to get the sync status.
  syncer::SyncService* _syncService;
  // Observer for sync service status changes.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // Observes changes to signed-in status.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // Used by SetUpList to get signed-in status.
  AuthenticationService* _authenticationService;
  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  SceneState* _sceneState;
  SetUpListMediatorObserverList* _observers;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                        syncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
              authenticationService:(AuthenticationService*)authService
                         sceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
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
    _prefChangeRegistrar.Init(_localState);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosCredentialProviderPromoLastActionTaken,
        &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kDisabled, &_prefChangeRegistrar);
    if (CredentialProviderPromoDismissed(_localState)) {
      set_up_list_prefs::MarkItemComplete(_localState,
                                          SetUpListItemType::kAutofill);
    } else {
      [self checkIfCPEEnabled];
    }

    _sceneState = sceneState;
    [_sceneState addObserver:self];

    _setUpList = [SetUpList buildFromPrefs:prefService
                                localState:_localState
                               syncService:syncService
                     authenticationService:authService];
    //        [_setUpList addObserver:self];
    _setUpList.delegate = self;

    _observers = [SetUpListMediatorObserverList
        observersWithProtocol:@protocol(SetUpListMediatorObserver)];
  }
  return self;
}

- (void)disconnect {
  _authenticationService = nullptr;
  _authServiceObserverBridge.reset();
  _syncObserverBridge.reset();
  _identityObserverBridge.reset();
  if (_prefObserverBridge) {
    _prefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
  [_setUpList disconnect];
  _setUpList = nil;
  [_sceneState removeObserver:self];
  _localState = nullptr;
}

- (void)addObserver:(id<SetUpListMediatorObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<SetUpListMediatorObserver>)observer {
  [_observers removeObserver:observer];
}

- (NSArray<SetUpListItemViewData*>*)allItems {
  NSMutableArray<SetUpListItemViewData*>* allItems =
      [[NSMutableArray alloc] init];
  for (SetUpListItem* model in _setUpList.allItems) {
    SetUpListItemViewData* item =
        [[SetUpListItemViewData alloc] initWithType:model.type
                                           complete:model.complete];
    [allItems addObject:item];
  }
  return allItems;
}

- (NSArray<SetUpListItemViewData*>*)setUpListItems {
  NSMutableArray<SetUpListItemViewData*>* items = [[NSMutableArray alloc] init];
  for (SetUpListItem* model in _setUpList.items) {
    SetUpListItemViewData* item =
        [[SetUpListItemViewData alloc] initWithType:model.type
                                           complete:model.complete];
    [items addObject:item];
  }
  return items;
}
- (BOOL)allItemsComplete {
  return [_setUpList allItemsComplete];
}

- (void)disableSetUpList {
  set_up_list_prefs::DisableSetUpList(_localState);
}

#pragma mark - SetUpListDelegate

- (void)setUpListItemDidComplete:(SetUpListItem*)item
               allItemsCompleted:(BOOL)completed {
  __weak __typeof(self) weakSelf = self;
  // Can resend signal to mediator from Set Up List after SetUpListItemView
  // completes animation
  ProceduralBlock completion = ^{
    if (completed) {
      [weakSelf.consumer showSetUpListDoneWithAnimations:^{
        if (!IsMagicStackEnabled()) {
          [weakSelf.delegate contentSuggestionsWasUpdated];
        }
      }];
    } else if (IsMagicStackEnabled()) {
      [weakSelf.consumer scrollToNextMagicStackModuleForCompletedModule:
                             SetUpListModuleTypeForSetUpListType(item.type)];
    }
  };
  [self.consumer markSetUpListItemComplete:item.type completion:completion];
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
  } else if (preferenceName == set_up_list_prefs::kDisabled &&
             set_up_list_prefs::IsSetUpListDisabled(_localState)) {
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

// Sets the pref for a SetUpList item to indicate it is complete.
- (void)markSetUpListItemPrefComplete:(SetUpListItemType)type {
  set_up_list_prefs::MarkItemComplete(_localState, type);
}

// Hides the Set Up List with an animation.
- (void)hideSetUpList {
  __weak __typeof(self) weakSelf = self;
  [self.consumer hideSetUpListWithAnimations:^{
    [weakSelf.delegate contentSuggestionsWasUpdated];
  }];
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

@end
