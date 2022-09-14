// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/public/base/signin_metrics.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace {

// Helper struct for computing the result of fetching account capabilities.
struct FetchCapabilitiesResult {
  FetchCapabilitiesResult() = default;
  ~FetchCapabilitiesResult() = default;

  ChromeIdentityCapabilityResult capability_value;
  signin_metrics::FetchAccountCapabilitiesFromSystemLibraryResult fetch_result;
};

// Computes the value of fetching account capabilities.
FetchCapabilitiesResult ComputeFetchCapabilitiesResult(
    NSNumber* capability_value,
    NSError* error) {
  FetchCapabilitiesResult result;
  if (error) {
    result.capability_value = ChromeIdentityCapabilityResult::kUnknown;
    result.fetch_result = signin_metrics::
        FetchAccountCapabilitiesFromSystemLibraryResult::kErrorGeneric;
  } else if (!capability_value) {
    result.capability_value = ChromeIdentityCapabilityResult::kUnknown;
    result.fetch_result =
        signin_metrics::FetchAccountCapabilitiesFromSystemLibraryResult::
            kErrorMissingCapability;
  } else {
    int capability_value_int = capability_value.intValue;
    switch (capability_value_int) {
      case static_cast<int>(ChromeIdentityCapabilityResult::kFalse):
      case static_cast<int>(ChromeIdentityCapabilityResult::kTrue):
      case static_cast<int>(ChromeIdentityCapabilityResult::kUnknown):
        result.capability_value =
            static_cast<ChromeIdentityCapabilityResult>(capability_value_int);
        result.fetch_result = signin_metrics::
            FetchAccountCapabilitiesFromSystemLibraryResult::kSuccess;
        break;
      default:
        result.capability_value = ChromeIdentityCapabilityResult::kUnknown;
        result.fetch_result =
            signin_metrics::FetchAccountCapabilitiesFromSystemLibraryResult::
                kErrorUnexpectedValue;
    }
  }
  return result;
}

}  // namespace

void ChromeIdentityService::Observer::OnAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  OnAccessTokenRefreshFailed(static_cast<id<SystemIdentity>>(identity),
                             user_info);
}

void ChromeIdentityService::Observer::OnProfileUpdate(
    ChromeIdentity* identity) {
  OnProfileUpdate(static_cast<id<SystemIdentity>>(identity));
}

ChromeIdentityService::ChromeIdentityService() {}

ChromeIdentityService::~ChromeIdentityService() {
  for (auto& observer : observer_list_)
    observer.OnChromeIdentityServiceWillBeDestroyed();
}

void ChromeIdentityService::DismissDialogs() {}

bool ChromeIdentityService::HandleSessionOpenURLContexts(UIScene* scene,
                                                         NSSet* URLContexts) {
  return false;
}

void ChromeIdentityService::ApplicationDidDiscardSceneSessions(
    NSSet* scene_sessions) {}

DismissASMViewControllerBlock
ChromeIdentityService::PresentAccountDetailsController(
    ChromeIdentity* identity,
    UIViewController* view_controller,
    BOOL animated) {
  return PresentAccountDetailsController(
      static_cast<id<SystemIdentity>>(identity), view_controller, animated);
}

DismissASMViewControllerBlock
ChromeIdentityService::PresentAccountDetailsController(
    id<SystemIdentity> identity,
    UIViewController* view_controller,
    BOOL animated) {
  return nil;
}

DismissASMViewControllerBlock
ChromeIdentityService::PresentWebAndAppSettingDetailsController(
    ChromeIdentity* identity,
    UIViewController* view_controller,
    BOOL animated) {
  return PresentWebAndAppSettingDetailsController(
      static_cast<id<SystemIdentity>>(identity), view_controller, animated);
}

DismissASMViewControllerBlock
ChromeIdentityService::PresentWebAndAppSettingDetailsController(
    id<SystemIdentity> identity,
    UIViewController* view_controller,
    BOOL animated) {
  return nil;
}

ChromeIdentityInteractionManager*
ChromeIdentityService::CreateChromeIdentityInteractionManager() const {
  NOTREACHED() << "Subclasses must override this";
  return nil;
}

void ChromeIdentityService::IterateOverIdentities(
    IdentityIteratorCallback callback) {
  IterateOverIdentities(base::BindRepeating(
      [](IdentityIteratorCallback callback, id<SystemIdentity> identity) {
        return callback.Run(
            base::mac::ObjCCastStrict<ChromeIdentity>(identity));
      },
      std::move(callback)));
}

void ChromeIdentityService::IterateOverIdentities(
    SystemIdentityIteratorCallback) {}

void ChromeIdentityService::ForgetIdentity(ChromeIdentity* identity,
                                           ForgetIdentityCallback callback) {
  ForgetIdentity(static_cast<id<SystemIdentity>>(identity), callback);
}

void ChromeIdentityService::ForgetIdentity(id<SystemIdentity> identity,
                                           ForgetIdentityCallback callback) {}

void ChromeIdentityService::GetAccessToken(ChromeIdentity* identity,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {
  GetAccessToken(static_cast<id<SystemIdentity>>(identity), scopes, callback);
}

void ChromeIdentityService::GetAccessToken(id<SystemIdentity> identity,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {}

void ChromeIdentityService::GetAccessToken(ChromeIdentity* identity,
                                           const std::string& client_id,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {
  GetAccessToken(static_cast<id<SystemIdentity>>(identity), client_id, scopes,
                 callback);
}

void ChromeIdentityService::GetAccessToken(id<SystemIdentity> identity,
                                           const std::string& client_id,
                                           const std::set<std::string>& scopes,
                                           AccessTokenCallback callback) {}

void ChromeIdentityService::GetAvatarForIdentity(ChromeIdentity* identity) {
  GetAvatarForIdentity(static_cast<id<SystemIdentity>>(identity));
}

void ChromeIdentityService::GetAvatarForIdentity(id<SystemIdentity> identity) {
  NOTREACHED();
}

UIImage* ChromeIdentityService::GetCachedAvatarForIdentity(
    ChromeIdentity* identity) {
  return GetCachedAvatarForIdentity(static_cast<id<SystemIdentity>>(identity));
}

UIImage* ChromeIdentityService::GetCachedAvatarForIdentity(
    id<SystemIdentity> identity) {
  NOTREACHED();
  return nil;
}

void ChromeIdentityService::GetHostedDomainForIdentity(
    ChromeIdentity* identity,
    GetHostedDomainCallback callback) {
  GetHostedDomainForIdentity(static_cast<id<SystemIdentity>>(identity),
                             callback);
}

void ChromeIdentityService::GetHostedDomainForIdentity(
    id<SystemIdentity> identity,
    GetHostedDomainCallback callback) {}

NSString* ChromeIdentityService::GetCachedHostedDomainForIdentity(
    ChromeIdentity* identity) {
  return GetCachedHostedDomainForIdentity(
      static_cast<id<SystemIdentity>>(identity));
}

NSString* ChromeIdentityService::GetCachedHostedDomainForIdentity(
    id<SystemIdentity> identity) {
  // @gmail.com accounts are end consumer accounts so it is safe to return @""
  // even when SSOProfileSource has a nil profile for `sso_identity`.
  //
  // Note: This is also needed during the sign-in flow as it avoids waiting for
  // the profile of `sso_identity` to be fetched from the server.
  if (gaia::ExtractDomainName(base::SysNSStringToUTF8(identity.userEmail)) ==
      "gmail.com") {
    return @"";
  }
  return nil;
}

void ChromeIdentityService::CanOfferExtendedSyncPromos(
    ChromeIdentity* identity,
    CapabilitiesCallback completion) {
  FetchCapability(identity, @(kCanOfferExtendedChromeSyncPromosCapabilityName),
                  completion);
}

void ChromeIdentityService::CanOfferExtendedSyncPromos(
    id<SystemIdentity> identity,
    CapabilitiesCallback completion) {
  FetchCapability(identity, @(kCanOfferExtendedChromeSyncPromosCapabilityName),
                  completion);
}

void ChromeIdentityService::IsSubjectToParentalControls(
    ChromeIdentity* identity,
    CapabilitiesCallback completion) {
  FetchCapability(identity, @(kIsSubjectToParentalControlsCapabilityName),
                  completion);
}

void ChromeIdentityService::IsSubjectToParentalControls(
    id<SystemIdentity> identity,
    CapabilitiesCallback completion) {
  FetchCapability(identity, @(kIsSubjectToParentalControlsCapabilityName),
                  completion);
}

bool ChromeIdentityService::IsServiceSupported() {
  return false;
}

MDMDeviceStatus ChromeIdentityService::GetMDMDeviceStatus(
    NSDictionary* user_info) {
  return 0;
}

bool ChromeIdentityService::HandleMDMNotification(ChromeIdentity* identity,
                                                  NSDictionary* user_info,
                                                  MDMStatusCallback callback) {
  return HandleMDMNotification(static_cast<id<SystemIdentity>>(identity),
                               user_info, callback);
}

bool ChromeIdentityService::HandleMDMNotification(id<SystemIdentity> identity,
                                                  NSDictionary* user_info,
                                                  MDMStatusCallback callback) {
  return false;
}

bool ChromeIdentityService::IsMDMError(ChromeIdentity* identity,
                                       NSError* error) {
  return IsMDMError(static_cast<id<SystemIdentity>>(identity), error);
}

bool ChromeIdentityService::IsMDMError(id<SystemIdentity> identity,
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

void ChromeIdentityService::FetchCapabilities(
    NSArray* capabilities,
    ChromeIdentity* identity,
    ChromeIdentityCapabilitiesFetchCompletionBlock completion) {
  FetchCapabilities(static_cast<id<SystemIdentity>>(identity), capabilities,
                    completion);
}

void ChromeIdentityService::FetchCapabilities(
    id<SystemIdentity> identity,
    NSArray<NSString*>* capabilities,
    ChromeIdentityCapabilitiesFetchCompletionBlock completion) {
  // Implementation provided by subclass.
}

void ChromeIdentityService::FireIdentityListChanged(bool notify_user) {
  for (auto& observer : observer_list_)
    observer.OnIdentityListChanged(notify_user);
}

void ChromeIdentityService::FireAccessTokenRefreshFailed(
    ChromeIdentity* identity,
    NSDictionary* user_info) {
  for (auto& observer : observer_list_)
    observer.OnAccessTokenRefreshFailed(identity, user_info);
}

void ChromeIdentityService::FireAccessTokenRefreshFailed(
    id<SystemIdentity> identity,
    NSDictionary* user_info) {
  FireAccessTokenRefreshFailed(
      base::mac::ObjCCastStrict<ChromeIdentity>(identity), user_info);
}

void ChromeIdentityService::FireProfileDidUpdate(ChromeIdentity* identity) {
  for (auto& observer : observer_list_)
    observer.OnProfileUpdate(identity);
}

void ChromeIdentityService::FireProfileDidUpdate(id<SystemIdentity> identity) {
  FireProfileDidUpdate(base::mac::ObjCCastStrict<ChromeIdentity>(identity));
}

void ChromeIdentityService::FetchCapability(ChromeIdentity* identity,
                                            NSString* capability_name,
                                            CapabilitiesCallback completion) {
  const base::TimeTicks fetch_start = base::TimeTicks::Now();
  FetchCapabilities(
      @[ capability_name ], identity,
      ^(NSDictionary<NSString*, NSNumber*>* capabilities, NSError* error) {
        base::UmaHistogramTimes(
            "Signin.AccountCapabilities.GetFromSystemLibraryDuration",
            base::TimeTicks::Now() - fetch_start);

        FetchCapabilitiesResult result = ComputeFetchCapabilitiesResult(
            [capabilities objectForKey:capability_name], error);
        base::UmaHistogramEnumeration(
            "Signin.AccountCapabilities.GetFromSystemLibraryResult",
            result.fetch_result);

        if (completion)
          completion(result.capability_value);
      });
}

void ChromeIdentityService::FetchCapability(id<SystemIdentity> identity,
                                            NSString* capability_name,
                                            CapabilitiesCallback completion) {
  const base::TimeTicks fetch_start = base::TimeTicks::Now();
  FetchCapabilities(
      identity, @[ capability_name ],
      ^(NSDictionary<NSString*, NSNumber*>* capabilities, NSError* error) {
        base::UmaHistogramTimes(
            "Signin.AccountCapabilities.GetFromSystemLibraryDuration",
            base::TimeTicks::Now() - fetch_start);

        FetchCapabilitiesResult result = ComputeFetchCapabilitiesResult(
            [capabilities objectForKey:capability_name], error);
        base::UmaHistogramEnumeration(
            "Signin.AccountCapabilities.GetFromSystemLibraryResult",
            result.fetch_result);

        if (completion)
          completion(result.capability_value);
      });
}

}  // namespace ios
