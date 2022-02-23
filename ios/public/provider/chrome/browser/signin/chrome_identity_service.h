// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_

#include <CoreFoundation/CoreFoundation.h>

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"

@class ChromeIdentity;
@class ChromeIdentityInteractionManager;
@class NSArray;
@class NSDate;
@class NSDictionary;
@class NSError;
@class NSString;
@class NSURL;
@class UIApplication;
@class UIImage;
@class UINavigationController;
@class UIScene;
@class UIViewController;

namespace ios {

class ChromeIdentityService;

// Callback passed to method |GetAccessTokenForScopes()| that returns the
// information of the obtained access token to the caller.
typedef void (^AccessTokenCallback)(NSString* token,
                                    NSDate* expiration,
                                    NSError* error);

// Callback passed to method |ForgetIdentity()|. |error| is nil if the operation
// completed with success.
typedef void (^ForgetIdentityCallback)(NSError* error);

// Callback passed to method |GetHostedDomainForIdentity()|.
// |hosted_domain|:
//   + nil, if error.
//   + an empty string, if this is a consumer account (e.g. foo@gmail.com).
//   + non-empty string, if the hosted domain was fetched and this account
//     has a hosted domain.
// |error|: Error if failed to fetch the identity profile.
typedef void (^GetHostedDomainCallback)(NSString* hosted_domain,
                                        NSError* error);

// Callback passed to method |HandleMDMNotification()|. |is_blocked| is true if
// the device is blocked.
typedef void (^MDMStatusCallback)(bool is_blocked);

// Callback to dismiss ASM view. No-op, if this block is more than once.
// |animated| the view will be dismissed with animation if the value is YES.
typedef void (^DismissASMViewControllerBlock)(BOOL animated);

// Defines account capability state based on GCRSSOCapabilityResult.
enum class ChromeIdentityCapabilityResult {
  // Capability is not allowed for identity.
  kFalse,
  // Capability is allowed for identity.
  kTrue,
  // Capability has not been set for identity.
  kUnknown,
};

// Callback to retrieve account capabilities. Maps |capability_result| to the
// corresponding state in ChromeIdentityCapabilityResult.
typedef void (^CapabilitiesCallback)(
    ChromeIdentityCapabilityResult capability_result);

// Callback for fetching the set of supported capabilities and their
// corresponding states as defined in ChromeIdentityCapabilityResult.
typedef void (^ChromeIdentityCapabilitiesFetchCompletionBlock)(
    NSDictionary* capabilities,
    NSError* error);

// Opaque type representing the MDM (Mobile Device Management) status of the
// device. Checking for equality is guaranteed to be valid.
typedef int MDMDeviceStatus;

// Value returned by IdentityIteratorCallback.
enum IdentityIteratorCallbackResult {
  kIdentityIteratorContinueIteration,
  kIdentityIteratorInterruptIteration,
};

// ChromeIdentityService abstracts the signin flow on iOS.
class ChromeIdentityService {
 public:
  // Observer handling events related to the ChromeIdentityService.
  class Observer {
   public:
    Observer() {}

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() {}

    // Handles identity list changed events.
    // |notify_user| is true if the identity list is updated by an external
    // source than Chrome. This means that a first party Google app had added or
    // removed identities, or the identity token is invalid.
    virtual void OnIdentityListChanged(bool notify_user) {}

    // Handles access token refresh failed events.
    // |identity| is the the identity for which the access token refresh failed.
    // |user_info| is the user info dictionary in the original notification. It
    // should not be accessed directly but via helper methods (like
    // ChromeIdentityService::IsInvalidGrantError).
    virtual void OnAccessTokenRefreshFailed(ChromeIdentity* identity,
                                            NSDictionary* user_info) {}

    // Called when profile information or the profile image is updated.
    virtual void OnProfileUpdate(ChromeIdentity* identity) {}

    // Called when the ChromeIdentityService will be destroyed.
    virtual void OnChromeIdentityServiceWillBeDestroyed() {}
  };

  // Callback invoked for each ChromeIdentity when iterating over them with
  // `IterateOverIdentities()`.
  using IdentityIteratorCallback =
      base::RepeatingCallback<IdentityIteratorCallbackResult(ChromeIdentity*)>;

  ChromeIdentityService();

  ChromeIdentityService(const ChromeIdentityService&) = delete;
  ChromeIdentityService& operator=(const ChromeIdentityService&) = delete;

  virtual ~ChromeIdentityService();

  // Handles open URL authentication callback. Returns whether the URL was
  // actually handled. This should be called within
  // -[<UISceneDelegate> application:openURLContexts:].
  virtual bool HandleSessionOpenURLContexts(UIScene* scene, NSSet* url_contexts)
      API_AVAILABLE(ios(13.0));

  // Discards scene session data.This should be called within
  // -[<UIApplicationDelegate> application:didDiscardSceneSessions:].
  virtual void ApplicationDidDiscardSceneSessions(NSSet* scene_sessions)
      API_AVAILABLE(ios(13.0));

  // Dismisses all the dialogs created by the abstracted flows.
  virtual void DismissDialogs();

  // Presents a new Account Details view.
  // |identity| the identity used to present the view.
  // |view_controller| the view to present the details view.
  // |animated| the view is presented with animation if YES.
  // Returns a block to dismiss the presented view. This block can be ignored if
  // not needed.
  virtual ios::DismissASMViewControllerBlock PresentAccountDetailsController(
      ChromeIdentity* identity,
      UIViewController* view_controller,
      BOOL animated);

  // Presents a new Web and App Setting Details view.
  // |identity| the identity used to present the view.
  // |view_controller| the view to present the setting details.
  // |animated| the view is presented with animation if YES.
  // Returns a block to dismiss the presented view. This block can be ignored if
  // not needed.
  virtual DismissASMViewControllerBlock
  PresentWebAndAppSettingDetailsController(ChromeIdentity* identity,
                                           UIViewController* view_controller,
                                           BOOL animated);

  // Returns a new ChromeIdentityInteractionManager with |delegate| as its
  // delegate.
  virtual ChromeIdentityInteractionManager*
  CreateChromeIdentityInteractionManager() const;

  // Iterates over all known ChromeIdentities, sorted by the ordering used
  // in account manager, which is typically based on the keychain ordering
  // of accounts.
  virtual void IterateOverIdentities(IdentityIteratorCallback callback);

  // Forgets the given identity on the device. This method logs the user out.
  // It is asynchronous because it needs to contact the server to revoke the
  // authentication token.
  // This may be called on an arbitrary thread, but callback will always be on
  // the main thread.
  virtual void ForgetIdentity(ChromeIdentity* identity,
                              ForgetIdentityCallback callback);

  // Asynchronously retrieves access tokens for the given identity and scopes.
  // Uses the default client id and client secret.
  virtual void GetAccessToken(ChromeIdentity* identity,
                              const std::set<std::string>& scopes,
                              AccessTokenCallback callback);

  // Asynchronously retrieves access tokens for the given identity and scopes.
  virtual void GetAccessToken(ChromeIdentity* identity,
                              const std::string& client_id,
                              const std::set<std::string>& scopes,
                              AccessTokenCallback callback);

  // Fetches the profile avatar, from the cache or the network.
  // For high resolution iPads, returns large images (200 x 200) to avoid
  // pixelization.
  // Observer::OnProfileUpdate() will be called when the avatar is available.
  virtual void GetAvatarForIdentity(ChromeIdentity* identity);

  // Synchronously returns any cached avatar, or nil.
  // GetAvatarForIdentity() should be generally used instead of this method.
  virtual UIImage* GetCachedAvatarForIdentity(ChromeIdentity* identity);

  // Fetches the identity hosted domain, from the cache or the network. Calls
  // back on the main thread.
  virtual void GetHostedDomainForIdentity(ChromeIdentity* identity,
                                          GetHostedDomainCallback callback);

  // Returns the identity hosted domain, for the cache only. This method
  // returns:
  //   + nil, if the hosted domain value was yet not fetched from the server.
  //   + an empty string, if this is a consumer account (e.g. foo@gmail.com).
  //   + non-empty string, if the hosted domain was fetched and this account
  //     has a hosted domain.
  virtual NSString* GetCachedHostedDomainForIdentity(ChromeIdentity* identity);

  // Asynchronously returns the value of the account capability that determines
  // whether Chrome should offer extended sync promos to |identity|. This value
  // will have a refresh period of 24 hours, meaning that at retrieval it may be
  // stale. If the value is not populated, as in a fresh install, the callback
  // will evaluate to false.
  void CanOfferExtendedSyncPromos(ChromeIdentity* identity,
                                  CapabilitiesCallback callback);

  // Asynchronously returns the value of the account capability that determines
  // whether parental controls should be applied to |identity|.
  void IsSubjectToParentalControls(ChromeIdentity* identity,
                                   CapabilitiesCallback callback);

  // Returns true if the service can be used, and supports ChromeIdentity list.
  virtual bool IsServiceSupported();

  // Returns the MDM device status associated with |user_info|.
  virtual MDMDeviceStatus GetMDMDeviceStatus(NSDictionary* user_info);

  // Handles a potential MDM (Mobile Device Management) notification. Returns
  // true if the notification linked to |identity| and |user_info| was an MDM
  // one. In this case, |callback| will be called later with the status of the
  // device.
  virtual bool HandleMDMNotification(ChromeIdentity* identity,
                                     NSDictionary* user_info,
                                     MDMStatusCallback callback);

  // Returns whether the |error| associated with |identity| is due to MDM
  // (Mobile Device Management).
  virtual bool IsMDMError(ChromeIdentity* identity, NSError* error);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether the given |user_info|, from an access token refresh failed
  // event, corresponds to an invalid grant error.
  virtual bool IsInvalidGrantError(NSDictionary* user_info);

 protected:
  // Asynchronously retrieves the list of supported capabilities for the given
  // Chrome identity.
  virtual void FetchCapabilities(
      NSArray* capabilities,
      ChromeIdentity* identity,
      ChromeIdentityCapabilitiesFetchCompletionBlock completion);
  // Fires |OnIdentityListChanged| on all observers.
  // |notify_user| is true if the identity list is updated by an external source
  // than Chrome. This means that a first party Google app had added or removed
  // identities, or the identity token is invalid.
  void FireIdentityListChanged(bool notify_user);

  // Fires |OnAccessTokenRefreshFailed| on all observers, with the corresponding
  // identity and user info.
  void FireAccessTokenRefreshFailed(ChromeIdentity* identity,
                                    NSDictionary* user_info);

  // Fires |OnProfileUpdate| on all observers, with the corresponding identity.
  void FireProfileDidUpdate(ChromeIdentity* identity);

 private:
  // Asynchronously retrieves the specified capability for the Chrome identity.
  void FetchCapability(ChromeIdentity* identity,
                       NSString* capability_name,
                       CapabilitiesCallback completion);

  base::ObserverList<Observer, true>::Unchecked observer_list_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_
