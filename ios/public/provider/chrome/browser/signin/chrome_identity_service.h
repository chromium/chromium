// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"

@class ChromeIdentity;
@protocol ChromeIdentityBrowserOpener;
@class ChromeIdentityInteractionManager;
@protocol ChromeIdentityInteractionManagerDelegate;
@class NSArray;
@class NSDate;
@class NSDictionary;
@class NSError;
@class NSString;
@class NSURL;
@class UIApplication;
@class UIImage;
@class UINavigationController;
@class UIViewController;

namespace ios {

class ChromeBrowserState;
class ChromeIdentityService;

// Callback passed to method |GetAccessTokenForScopes()| that returns the
// information of the obtained access token to the caller.
typedef void (^AccessTokenCallback)(NSString* token,
                                    NSDate* expiration,
                                    NSError* error);

// Callback passed to method |ForgetIdentity()|. |error| is nil if the operation
// completed with success.
typedef void (^ForgetIdentityCallback)(NSError* error);

// Callback passed to method |GetAvatarForIdentity()|.
typedef void (^GetAvatarCallback)(UIImage* avatar);

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

// Opaque type representing the MDM (Mobile Device Management) status of the
// device. Checking for equality is guaranteed to be valid.
typedef int MDMDeviceStatus;

// ChromeIdentityService abstracts the signin flow on iOS.
class ChromeIdentityService {
 public:
  // Observer handling events related to the ChromeIdentityService.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    // Handles identity list changed events.
    virtual void OnIdentityListChanged() {}

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

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  ChromeIdentityService();
  virtual ~ChromeIdentityService();

  // Handles open URL authentication callback. Returns whether the URL was
  // actually handled. This should be called within
  // UIApplicationDelegate application:openURL:options:.
  virtual bool HandleApplicationOpenURL(UIApplication* application,
                                        NSURL* url,
                                        NSDictionary* options);

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
  CreateChromeIdentityInteractionManager(
      ios::ChromeBrowserState* browser_state,
      id<ChromeIdentityInteractionManagerDelegate> delegate) const;

  // Returns YES if |identity| is valid and if the service has it in its list of
  // identitites.
  virtual bool IsValidIdentity(ChromeIdentity* identity) const;

  // Returns the chrome identity having the email equal to |email| or |nil| if
  // no matching identity is found.
  virtual ChromeIdentity* GetIdentityWithEmail(const std::string& email) const;

  // Returns the chrome identity having the gaia ID equal to |gaia_id| or |nil|
  // if no matching identity is found.
  virtual ChromeIdentity* GetIdentityWithGaiaID(
      const std::string& gaia_id) const;

  // Returns the canonicalized emails for all identities.
  virtual std::vector<std::string> GetCanonicalizeEmailsForAllIdentities()
      const;

  // Returns true if there is at least one identity.
  virtual bool HasIdentities() const;

  // Returns all ChromeIdentity objects in an array.
  virtual NSArray* GetAllIdentities() const;

  // Returns all ChromeIdentity objects sorted by the ordering used in the
  // account manager, which is typically based on the keychain ordering of
  // accounts.
  virtual NSArray* GetAllIdentitiesSortedForDisplay() const;

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
  // pixelization. Calls back on the main thread. |callback| may be nil.
  virtual void GetAvatarForIdentity(ChromeIdentity* identity,
                                    GetAvatarCallback callback);

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

  // Retuns the MDM device status associated with |user_info|.
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
  // Fires |OnIdentityListChanged| on all observers.
  void FireIdentityListChanged();

  // Fires |OnAccessTokenRefreshFailed| on all observers, with the corresponding
  // identity and user info.
  void FireAccessTokenRefreshFailed(ChromeIdentity* identity,
                                    NSDictionary* user_info);

  // Fires |OnProfileUpdate| on all observers, with the corresponding identity.
  void FireProfileDidUpdate(ChromeIdentity* identity);

 private:
  base::ObserverList<Observer, true>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeIdentityService);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_
