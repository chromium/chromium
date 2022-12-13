// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SYSTEM_IDENTITY_MANAGER_H_
#define IOS_CHROME_BROWSER_SIGNIN_SYSTEM_IDENTITY_MANAGER_H_

#import <UIKit/UIKit.h>

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "ios/chrome/browser/signin/system_identity_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

@protocol RefreshAccessTokenError;
@protocol SystemIdentity;
@protocol SystemIdentityInteractionManager;
class SystemIdentityManagerObserver;

// SystemIdentityManager abstracts the signin flow on iOS.
class SystemIdentityManager {
 public:
  // Value returned by IdentityIteratorCallback.
  enum class IteratorResult {
    kContinueIteration,
    kInterruptIteration,
  };

  // Value representing account capabilities. The enumerator values must not
  // be changed as they correspond to the value exchanged on the wire with
  // the server.
  enum class CapabilityResult {
    kFalse = 0,    // Capability not allowed for identity.
    kTrue = 1,     // Capability allowed for identity.
    kUnknown = 2,  // Capability not set for identity.
  };

  // Value representing a refresh access token.
  struct AccessTokenInfo {
    // The access token itself.
    std::string token;

    // The time at which this access token will expire. This will be set to the
    // NULL time value of `base::Time()` when no expiration time is available.
    base::Time expiration_time;
  };

  // Callback invoked for each id<SystemIdentity> when iterating over them
  // with `IterateOverIdentities()`. The returned value can be used to stop
  // the iteration prematurely.
  using IdentityIteratorCallback =
      base::RepeatingCallback<IteratorResult(id<SystemIdentity>)>;

  // Callback returned when presenting an account detail view. This callback
  // can be invoked to dismiss the view (with animation if `animated` is true).
  using DismissViewCallback = base::OnceCallback<void(bool animated)>;

  // Callback invoked when the `ForgetIdentity()` operation completes.
  using ForgetIdentityCallback = base::OnceCallback<void(NSError*)>;

  // Callback invoked when the `GetAccessToken()` operation completes.
  using AccessTokenCallback =
      base::OnceCallback<void(absl::optional<AccessTokenInfo>, NSError*)>;

  // Callback invoked when the `GetHostedDomain()` operation completes.
  using HostedDomainCallback = base::OnceCallback<void(NSString*, NSError*)>;

  // Callback invoked when the `CanOfferExtendedSyncPromos()` or
  // `IsSubjectToParentalControls()` operations complete.
  using FetchCapabilityCallback = base::OnceCallback<void(CapabilityResult)>;

  // Callback invoked when the `FetchCapabilitie()` operation completes.
  using FetchCapabilitiesCallback =
      base::OnceCallback<void(std::map<std::string, CapabilityResult>)>;

  // Callback invoked when `HandleMDMNotification` completes. Is is invoked
  // with a boolean indicating whether the device is blocked or not.
  using HandleMDMCallback = base::OnceCallback<void(bool)>;

  SystemIdentityManager();

  SystemIdentityManager(const SystemIdentityManager&) = delete;
  SystemIdentityManager& operator=(const SystemIdentityManager&) = delete;

  virtual ~SystemIdentityManager();

  // Asynchronously returns the value of the account capability that determines
  // whether Chrome should offer extended sync promos to `identity`. This value
  // will have a refresh period of 24 hours, meaning that at retrieval it may be
  // stale. If the value is not populated, as in a fresh install, the capability
  // will be considered as not allowed for identity.
  //
  // This is a wrapper around `FetchCapabilities()`.
  void CanOfferExtendedSyncPromos(id<SystemIdentity> identity,
                                  FetchCapabilityCallback callback);

  // Asynchronously returns the value of the account capability that determines
  // whether parental controls should be applied to `identity`.
  //
  // This is a wrapper around `FetchCapabilities()`.
  void IsSubjectToParentalControls(id<SystemIdentity> identity,
                                   FetchCapabilityCallback callback);

  // Adds/removes observers.
  void AddObserver(SystemIdentityManagerObserver* observer);
  void RemoveObserver(SystemIdentityManagerObserver* observer);

  // Handles open URL authentication callback. Should be called within
  // `-[UISceneDelegate application:openURLContexts:]` context. Returns
  // whether one the URLs was actually handled.
  virtual bool HandleSessionOpenURLContexts(
      UIScene* scene,
      NSSet<UIOpenURLContext*>* url_contexts) = 0;

  // Discards scene session data. Should be called within
  // `-[UIApplicationDelegate application:didDiscardSceneSessions:]`.
  virtual void ApplicationDidDiscardSceneSessions(
      NSSet<UISceneSession*>* scene_sessions) = 0;

  // Dismisses all the dialogs created by the abstracted flows.
  virtual void DismissDialogs() = 0;

  // Presents a new Account Details view and returns a callback that can be
  // used to dismiss the view (can be ignore if not needed). `identity` is the
  // identity used to present the view, `view_controller` is the view used to
  // present the details, `animated` controls whether the view is presented
  // with an animation
  virtual DismissViewCallback PresentAccountDetailsController(
      id<SystemIdentity> identity,
      UIViewController* view_controller,
      bool animated) = 0;

  // Presents a new Web and App Setting Details view and returns a callback
  // that can be used to dismiss the view (can be ignore if not needed).
  // `identity` is the identity used to present the view, `view_controller`
  // is the view used to present the details, `animated` controls whether the
  // view is presented with an animation.
  virtual DismissViewCallback PresentWebAndAppSettingDetailsController(
      id<SystemIdentity> identity,
      UIViewController* view_controller,
      bool animated) = 0;

  // Creates a new SystemIdentityInteractionManager instance.
  virtual id<SystemIdentityInteractionManager> CreateInteractionManager() = 0;

  // Iterates over all known identities, sortted by the ordering used in
  // account manager, which is typically based on the keychain ordering
  // of the accounts.
  virtual void IterateOverIdentities(IdentityIteratorCallback callback) = 0;

  // Asynchronously forgets `identity` and logs the user out. The callback
  // is invoked on the calling sequence when the operation completes.
  virtual void ForgetIdentity(id<SystemIdentity> identity,
                              ForgetIdentityCallback callback) = 0;

  // Asynchronously retrieves access tokens for `identity` with `scopes`. The
  // callback is invoked on the calling sequence when the operation completes.
  // Uses the default client id and client secret.
  virtual void GetAccessToken(id<SystemIdentity> identity,
                              const std::set<std::string>& scopes,
                              AccessTokenCallback callback) = 0;

  // Asynchronously retrieves access tokens for `identity` with `scopes`. The
  // callback is invoked on the calling sequence when the operation completes.
  virtual void GetAccessToken(id<SystemIdentity> identity,
                              const std::string& client_id,
                              const std::set<std::string>& scopes,
                              AccessTokenCallback callback) = 0;

  // Asynchronously fetches the avatar for `identity` from the network and
  // store it in the cache. The image can be large to avoid pixelation on
  // high resolution devices. Observers will be notified when the avatar is
  // available by the `OnIdentityUpdated()` method.
  virtual void FetchAvatarForIdentity(id<SystemIdentity> identity) = 0;

  // Synchronously returns the last cached avatar for `identity`. Should be
  // preceded by a call to `FetchAvatarForIdentity()` to populate the cache.
  virtual UIImage* GetCachedAvatarForIdentity(id<SystemIdentity> identity) = 0;

  // Asynchronously fetch the identity hosted domain. The callback is invoked
  // on the calling sequence when the operation completes.
  virtual void GetHostedDomain(id<SystemIdentity> identity,
                               HostedDomainCallback callback) = 0;

  // Returns the hosted domain for `identity` from the cache. Returns:
  //   + nil if the hosted domain value has not been fetched from the server,
  //   + an empty string if this is a consumer account (e.g. foo@gmail.com),
  //   + the hosted domain as a non-empty string otherwise.
  virtual NSString* GetCachedHostedDomainForIdentity(
      id<SystemIdentity> identity) = 0;

  // Asynchronously returns the capabilities for `identity`.
  virtual void FetchCapabilities(id<SystemIdentity> identity,
                                 const std::set<std::string>& names,
                                 FetchCapabilitiesCallback callback) = 0;

  // Asynchronously handles a potential MDM (Mobile Device Management) event.
  // The callback is invoked on the calling sequence when the operation
  // completes.
  virtual bool HandleMDMNotification(id<SystemIdentity> identity,
                                     id<RefreshAccessTokenError> error,
                                     HandleMDMCallback callback) = 0;

  // Returns whether the `error` associated with `identity` is due to MDM
  // (Mobile Device Management) or not.
  virtual bool IsMDMError(id<SystemIdentity> identity, NSError* error) = 0;

 protected:
  // Invokes `OnIdentityListChanged(...)` for all observers.
  void FireIdentityListChanged(bool notify_user);

  // Invokes `OnIdentityUpdated(...)` for all observers.
  void FireIdentityUpdated(id<SystemIdentity> identity);

  // Invokes OnIdentityAccessTokenRefreshFailed(...)` for all observers.
  void FireIdentityAccessTokenRefreshFailed(id<SystemIdentity> identity,
                                            id<RefreshAccessTokenError> error);

  // The SystemIdentityManager is sequence-affine. This is protected to
  // allow sub-classes access to the member field for use in DCHECK().
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Registered observers.
  base::ObserverList<SystemIdentityManagerObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_SYSTEM_IDENTITY_MANAGER_H_
