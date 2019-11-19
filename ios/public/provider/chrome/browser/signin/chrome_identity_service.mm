// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#include "base/strings/sys_string_conversions.h"
#include "google_apis/gaia/gaia_auth_util.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

ChromeIdentityService::ChromeIdentityService() {}

ChromeIdentityService::~ChromeIdentityService() {
  for (auto& observer : observer_list_)
    observer.OnChromeIdentityServiceWillBeDestroyed();
}

void ChromeIdentityService::DismissDialogs() {}

bool ChromeIdentityService::HandleApplicationOpenURL(UIApplication* application,
                                                     NSURL* url,
                                                     NSDictionary* options) {
  return false;
}

DismissASMViewControllerBlock
ChromeIdentityService::PresentAccountDetailsController(
    ChromeIdentity* identity,
    UIViewController* view_controller,
    BOOL animated) {
  return nil;
}

DismissASMViewControllerBlock
ChromeIdentityService::PresentWebAndAppSettingDetailsController(
    ChromeIdentity* identity,
    UIViewController* view_controller,
    BOOL animated) {
  return nil;
}

ChromeIdentityInteractionManager*
ChromeIdentityService::CreateChromeIdentityInteractionManager(
    ios::ChromeBrowserState* browser_state,
    id<ChromeIdentityInteractionManagerDelegate> delegate) const {
  return nil;
}

bool ChromeIdentityService::IsValidIdentity(ChromeIdentity* identity) const {
  return false;
}

ChromeIdentity* ChromeIdentityService::GetIdentityWithEmail(
    const std::string& email) const {
  return nil;
}

ChromeIdentity* ChromeIdentityService::GetIdentityWithGaiaID(
    const std::string& gaia_id) const {
  return nil;
}

std::vector<std::string>
ChromeIdentityService::GetCanonicalizeEmailsForAllIdentities() const {
  return std::vector<std::string>();
}

bool ChromeIdentityService::HasIdentities() const {
  return false;
}

NSArray* ChromeIdentityService::GetAllIdentities() const {
  return nil;
}

NSArray* ChromeIdentityService::GetAllIdentitiesSortedForDisplay() const {
  return nil;
}

void ChromeIdentityService::ForgetIdentity(ChromeIdentity* identity,
                                           ForgetIdentityCallback callback) {}

void ChromeIdentityService::GetAccessToken(ChromeIdentity* identity,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {}

void ChromeIdentityService::GetAccessToken(ChromeIdentity* identity,
                                           const std::string& client_id,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {}

void ChromeIdentityService::GetAvatarForIdentity(ChromeIdentity* identity,
                                                 GetAvatarCallback callback) {}

UIImage* ChromeIdentityService::GetCachedAvatarForIdentity(
    ChromeIdentity* identity) {
  return nil;
}

void ChromeIdentityService::GetHostedDomainForIdentity(
    ChromeIdentity* identity,
    GetHostedDomainCallback callback) {}

NSString* ChromeIdentityService::GetCachedHostedDomainForIdentity(
    ChromeIdentity* identity) {
  // @gmail.com accounts are end consumer accounts so it is safe to return @""
  // even when SSOProfileSource has a nil profile for |sso_identity|.
  //
  // Note: This is also needed during the sign-in flow as it avoids waiting for
  // the profile of |sso_identity| to be fetched from the server.
  if (gaia::ExtractDomainName(base::SysNSStringToUTF8(identity.userEmail)) ==
      "gmail.com") {
    return @"";
  }
  return nil;
}

MDMDeviceStatus ChromeIdentityService::GetMDMDeviceStatus(
    NSDictionary* user_info) {
  return 0;
}

bool ChromeIdentityService::HandleMDMNotification(ChromeIdentity* identity,
                                                  NSDictionary* user_info,
                                                  MDMStatusCallback callback) {
  return false;
}

bool ChromeIdentityService::IsMDMError(ChromeIdentity* identity,
                                       NSError* error) {
  return false;
}

void ChromeIdentityService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromeIdentityService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ChromeIdentityService::IsInvalidGrantError(NSDictionary* user_info) {
  return false;
}

void ChromeIdentityService::FireIdentityListChanged() {
  for (auto& observer : observer_list_)
    observer.OnIdentityListChanged();
}

void ChromeIdentityService::FireAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  for (auto& observer : observer_list_)
    observer.OnAccessTokenRefreshFailed(identity, user_info);
}

void ChromeIdentityService::FireProfileDidUpdate(ChromeIdentity* identity) {
  for (auto& observer : observer_list_)
    observer.OnProfileUpdate(identity);
}

}  // namespace ios
