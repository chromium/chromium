// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_MANAGER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_MANAGER_H_

#import <Foundation/Foundation.h>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "google_apis/gaia/core_account_id.h"
#include "ios/chrome/browser/signin/model/fake_system_identity_details.h"
#include "ios/chrome/browser/signin/model/system_identity_interaction_manager.h"
#include "ios/chrome/browser/signin/model/system_identity_manager.h"
#include "ios/chrome/browser/signin/model/system_identity_manager_observer.h"

@class FakeSystemIdentity;
@class FakeSystemIdentityManagerStorage;
@protocol SystemIdentity;

// An implementation of SystemIdentityManager that is used during test.
// It allows faking the list of identities available on the system.
class FakeSystemIdentityManager final : public SystemIdentityManager {
 public:
  // Callback used to mock HandleMDMNotification for errors created with
  // `CreateRefreshAccessTokenFailure()`.
  using HandleMDMNotificationCallback =
      base::RepeatingCallback<void(HandleMDMCallback)>;

  FakeSystemIdentityManager();
  explicit FakeSystemIdentityManager(
      NSArray<FakeSystemIdentity*>* fake_identities);
  ~FakeSystemIdentityManager() final;

  // Converts `manager` into a `FakeSystemIdentityManager*` if possible
  // or fail if the conversion is not valid. Must be used to get access
  // to FakeSystemIdentityManager API in tests.
  static FakeSystemIdentityManager* FromSystemIdentityManager(
      SystemIdentityManager* manager);

  // Adds `identity` to the available idendities.
  // DCHECK failure will be triggered if the identity was already added.
  void AddIdentity(id<SystemIdentity> identity);

  // Adds `identity` to the available idendities without setting up
  // capabilities.
  // DCHECK failure will be triggered if the identity was already added.
  void AddIdentityWithUnknownCapabilities(id<SystemIdentity> identity);

  // Adds `identity` and set the capabilities before firing the list changed
  // notification.
  // DCHECK failure will be triggered if the identity was already added.
  void AddIdentityWithCapabilities(
      id<SystemIdentity> identity,
      NSDictionary<NSString*, NSNumber*>* capabilities);

  // Simulates `identity` removed from another application.
  void ForgetIdentityFromOtherApplication(id<SystemIdentity> identity);

  // Returns a test object that enables changes to capability state.
  AccountCapabilitiesTestMutator* GetPendingCapabilitiesMutator(
      id<SystemIdentity> identity);

  // Returns the list of account capabilities associated with the identity.
  AccountCapabilities GetVisibleCapabilities(id<SystemIdentity> identity);

  // Sets whether the hosted domain for each identity will be automatically and
  // immediately available via GetCachedHostedDomainForIdentity(). If false, the
  // hosted domain must first be queried (asynchronously) via GetHostedDomain().
  // True by default.
  void SetInstantlyFillHostedDomainCache(bool instantly_fill);

  // Sets the error to be returned from all GetHostedDomain() calls. If nil, the
  // calls will succeed.
  void SetGetHostedDomainError(NSError* error);

  // Returns the number of hosted domain requests that have been answered with
  // an error (as set by SetGetHostedDomainError()).
  size_t GetNumHostedDomainErrorsReturned() const;

  // Simulates reloading the identities from the keychain.
  void FireSystemIdentityReloaded();

  // Simulates an updated notification for `identity`.
  void FireIdentityUpdatedNotification(id<SystemIdentity> identity);

  // Simulates a refresh token updated notification for `identity`.
  void FireIdentityRefreshTokenUpdatedNotification(id<SystemIdentity> identity);

  // Waits until all asynchronous callbacks have been completed.
  void WaitForServiceCallbacksToComplete();

  // Returns YES if the identity was already added.
  bool ContainsIdentity(id<SystemIdentity> identity);

  // Simulates a persistent authentication error for an account. After calling
  // this method, token requests for the corresponding account will fail with an
  // auth error.
  void SetPersistentAuthErrorForAccount(const CoreAccountId& accountId);

  // Simulates a persistent authentication error being resolved for an account.
  // After calling this method, token requests for the corresponding account
  // will succeed.
  void ClearPersistentAuthErrorForAccount(const CoreAccountId& accountId);

  // Sets a callback to be called whenever an access token the specified
  // account is requested.
  void SetGetAccessTokenCallback(const CoreAccountId& accountId,
                                 GetAccessTokenCallback callback);

  // Simulates a failure next time the access token for `identity` would be
  // fetched and return the error that would be sent to the observers. The
  // callback will be invoked each time `HandleMDMNotification()` is called
  // with the returned error object.
  //
  // Only a single error can be scheduled, so if this method is called a
  // second time without refreshing the access token, only the second error
  // will be sent to the observers.
  id<RefreshAccessTokenError> CreateRefreshAccessTokenFailure(
      id<SystemIdentity> identity,
      HandleMDMNotificationCallback callback);

  // SystemIdentityManager implementation.
  bool IsSigninSupported() final;
  bool HandleSessionOpenURLContexts(
      UIScene* scene,
      NSSet<UIOpenURLContext*>* url_contexts) final;
  void ApplicationDidDiscardSceneSessions(
      NSSet<UISceneSession*>* scene_sessions) final;
  void DismissDialogs() final;
  DismissViewCallback PresentAccountDetailsController(
      PresentDialogConfiguration configuration) final;
  DismissViewCallback PresentWebAndAppSettingDetailsController(
      PresentDialogConfiguration configuration) final;
  DismissViewCallback PresentLinkedServicesSettingsDetailsController(
      PresentDialogConfiguration configuration) final;

  // Sets the factory for creating SystemIdentityInteractionManager instances.
  void SetInteractionManagerFactory(
      base::RepeatingCallback<id<SystemIdentityInteractionManager>()> factory);
  id<SystemIdentityInteractionManager> CreateInteractionManager() final;

  void IterateOverIdentities(IdentityIteratorCallback callback) final;
  void ForgetIdentity(id<SystemIdentity> identity,
                      ForgetIdentityCallback callback) final;
  bool IdentityRemovedByUser(const GaiaId& gaia_id) final;
  void GetAccessToken(id<SystemIdentity> identity,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) final;
  void GetAccessToken(id<SystemIdentity> identity,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) final;
  void FetchAvatarForIdentity(id<SystemIdentity> identity) final;
  UIImage* GetCachedAvatarForIdentity(id<SystemIdentity> identity) final;
  void GetHostedDomain(id<SystemIdentity> identity,
                       HostedDomainCallback callback) final;
  NSString* GetCachedHostedDomainForIdentity(id<SystemIdentity> identity) final;
  void FetchCapabilities(id<SystemIdentity> identity,
                         const std::vector<std::string>& names,
                         FetchCapabilitiesCallback callback) final;
  bool HandleMDMNotification(id<SystemIdentity> identity,
                             NSArray<id<SystemIdentity>>* active_identities,
                             id<RefreshAccessTokenError> error,
                             HandleMDMCallback callback) final;
  bool IsScopeLimitedError(id<RefreshAccessTokenError> error) final;
  bool IsMDMError(id<SystemIdentity> identity, NSError* error) final;
  void FetchTokenAuthURL(id<SystemIdentity> identity,
                         NSURL* target_url,
                         AuthenticatedURLCallback callback) final;

 private:
  // Returns a weak pointer to the current instance.
  base::WeakPtr<FakeSystemIdentityManager> GetWeakPtr();

  // Helper used to implement the asynchronous part of `ForgetIdentity`.
  void ForgetIdentityAsync(id<SystemIdentity> identity,
                           ForgetIdentityCallback callback,
                           bool removed_by_user);

  // Helper used to implement the asynchronous part of `GetAccessToken`.
  void GetAccessTokenAsync(id<SystemIdentity> identity,
                           AccessTokenCallback callback);

  // Helper used to implement the asynchronous part of `FetchAvatarForIdentity`.
  void FetchAvatarForIdentityAsync(id<SystemIdentity> identity);

  // Helper used to implement the asynchronous part of `GetHostedDomain`.
  void GetHostedDomainAsync(id<SystemIdentity> identity,
                            HostedDomainCallback callback);

  // Helper used to implement the asynchronous part of `GetHostedDomain`.
  void FetchCapabilitiesAsync(id<SystemIdentity> identity,
                              const std::vector<std::string>& names,
                              FetchCapabilitiesCallback callback);

  // Posts `closure` to be executed asynchronously on the current sequence
  // while maintaining a counter of pending callbacks. The counter is used
  // to implement `WaitForServiceCallbacksToComplete()`.
  void PostClosure(base::Location from_here, base::OnceClosure closure);

  // Runs `closure` updating the counter of pending callbaks. Resume the
  // execution of `WaitForServiceCallbacksToComplete()` when the counter
  // reaches 0.
  void ExecuteClosure(base::OnceClosure closure);

  bool instantly_fill_hosted_domain_cache_ = true;
  NSMutableSet<id<SystemIdentity>>* hosted_domain_cache_ = [NSMutableSet set];
  NSError* get_hosted_domain_error_ = nil;
  size_t num_hosted_domain_errors_returned_ = 0;

  // Counter of pending callback and closure used to resume the execution
  // of `WaitForServiceCallbacksToComplete()` when the counter reaches 0.
  size_t pending_callbacks_ = 0;
  base::OnceClosure resume_closure_;

  // Stores identities.
  __strong FakeSystemIdentityManagerStorage* storage_ = nil;
  // List of gaia ids for identities that has been removed by calling
  // `ForgetIdentity()` (instead of `ForgetIdentityFromOtherApplication()`).
  base::flat_set<GaiaId> gaia_ids_removed_by_user_;

  base::RepeatingCallback<id<SystemIdentityInteractionManager>()>
      interaction_manager_factory_;

  base::WeakPtrFactory<FakeSystemIdentityManager> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_MANAGER_H_
