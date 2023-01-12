// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_MANAGER_H_
#define IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_MANAGER_H_

#import <Foundation/Foundation.h>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/signin/system_identity_manager.h"
#include "ios/chrome/browser/signin/system_identity_manager_observer.h"

@protocol SystemIdentity;
@class FakeSystemIdentityManagerStorage;

// An implementation of SystemIdentityManager that is used during test.
// It allows faking the list of identities available on the system.
class FakeSystemIdentityManager final : public SystemIdentityManager {
 public:
  // Callback used to mock HandleMDMNotification for errors created with
  // `CreateRefreshAccessTokenFailure()`.
  using HandleMDMNotificationCallback =
      base::RepeatingCallback<void(HandleMDMCallback)>;

  FakeSystemIdentityManager();
  explicit FakeSystemIdentityManager(NSArray<id<SystemIdentity>>* identities);
  ~FakeSystemIdentityManager() final;

  // Converts `manager` into a `FakeSystemIdentityManager*` if possible
  // or fail if the conversion is not valid. Must be used to get access
  // to FakeSystemIdentityManager API in tests.
  static FakeSystemIdentityManager* FromSystemIdentityManager(
      SystemIdentityManager* manager);

  // Adds `identity` to the available idendities.
  void AddIdentity(id<SystemIdentity> identity);

  // Adds fake identities given their names.
  void AddIdentities(NSArray<NSString*>* names);

  // Adds fake managed identities given their names.
  void AddManagedIdentities(NSArray<NSString*>* names);

  // Simulates `identity` removed from another application.
  void ForgetIdentityFromOtherApplication(id<SystemIdentity> identity);

  // Adds a `capabilities` for `identity`. Requires the identity to have been
  // added to the available identites first.
  void SetCapabilities(
      id<SystemIdentity> identity,
      const std::map<std::string, CapabilityResult>& capabilities);

  // Simulates reloading the identities from the keychain.
  void FireSystemIdentityReloaded();

  // Simulates an updated notification for `identity`.
  void FireIdentityUpdatedNotification(id<SystemIdentity> identity);

  // Waits until all asynchronous callbacks have been completed.
  void WaitForServiceCallbacksToComplete();

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
      id<SystemIdentity> identity,
      UIViewController* view_controller,
      bool animated) final;
  DismissViewCallback PresentWebAndAppSettingDetailsController(
      id<SystemIdentity> identity,
      UIViewController* view_controller,
      bool animated) final;
  id<SystemIdentityInteractionManager> CreateInteractionManager() final;
  void IterateOverIdentities(IdentityIteratorCallback callback) final;
  void ForgetIdentity(id<SystemIdentity> identity,
                      ForgetIdentityCallback callback) final;
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
                         const std::set<std::string>& names,
                         FetchCapabilitiesCallback callback) final;
  bool HandleMDMNotification(id<SystemIdentity> identity,
                             id<RefreshAccessTokenError> error,
                             HandleMDMCallback callback) final;
  bool IsMDMError(id<SystemIdentity> identity, NSError* error) final;

 private:
  // Returns a weak pointer to the current instance.
  base::WeakPtr<FakeSystemIdentityManager> GetWeakPtr();

  // Helper used to implement the asynchronous part of `ForgetIdentity`.
  void ForgetIdentityAsync(id<SystemIdentity> identity,
                           ForgetIdentityCallback callback,
                           bool notify_user);

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
                              const std::set<std::string>& names,
                              FetchCapabilitiesCallback callback);

  // Posts `closure` to be executed asynchronously on the current sequence
  // while maintaining a counter of pending callbacks. The counter is used
  // to implement `WaitForServiceCallbacksToComplete()`.
  void PostClosure(base::Location from_here, base::OnceClosure closure);

  // Runs `closure` updating the counter of pending callbaks. Resume the
  // execution of `WaitForServiceCallbacksToComplete()` when the counter
  // reaches 0.
  void ExecuteClosure(base::OnceClosure closure);

  // Counter of pending callback and closure used to resume the execution
  // of `WaitForServiceCallbacksToComplete()` when the counter reaches 0.
  size_t pending_callbacks_ = 0;
  base::OnceClosure resume_closure_;

  // Stores identities.
  __strong FakeSystemIdentityManagerStorage* storage_ = nil;

  base::WeakPtrFactory<FakeSystemIdentityManager> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_MANAGER_H_
