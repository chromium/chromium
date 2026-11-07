// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_policy_util.h"

#import "components/policy/core/common/management/management_service.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace {

// Policy integer value for `prefs::kGeminiActOnWebSettings`.
constexpr int kGeminiActOnWebSettingsEnabled = 0;

// Returns whether the browser, device, or account is enterprise-managed, or
// `kGeminiSparkSettings` is explicitly managed by policy.
bool IsManaged(ProfileIOS* profile, const PrefService* prefs) {
  policy::BrowserManagementService* browser_management =
      policy::BrowserManagementServiceFactory::GetForProfile(profile);
  policy::ManagementService* platform_management =
      policy::BrowserManagementServiceFactory::GetForPlatform();
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);

  return (browser_management && browser_management->IsManaged()) ||
         (platform_management && platform_management->IsManaged()) ||
         (auth_service && auth_service->HasPrimaryIdentityManaged()) ||
         prefs->IsManagedPreference(prefs::kGeminiSparkSettings);
}

// Returns whether the client is a likely Google dogfood client.
bool IsLikelyDogfoodClient() {
  ApplicationContext* app_context = GetApplicationContext();
  variations::VariationsService* variations_service =
      app_context ? app_context->GetVariationsService() : nullptr;
  return variations_service && variations_service->IsLikelyDogfoodClient();
}

}  // namespace

bool IsRemoteActorCredentialSharingAllowedByPolicy(ProfileIOS* profile) {
  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }

  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return false;
  }

  // Prerequisite Gemini and Act-on-Web policies default to allowed (`0`), so
  // check them directly.
  if (!gemini::GeminiAllowedByPolicy(prefs) ||
      prefs->GetInteger(prefs::kGeminiActOnWebSettings) !=
          kGeminiActOnWebSettingsEnabled) {
    return false;
  }

  // `GeminiSparkSettings` defaults to disabled (`1`) and is enforced only for
  // managed users/devices (exempting dogfood clients, matching Desktop Glic).
  if (!IsManaged(profile, prefs) || IsLikelyDogfoodClient()) {
    return true;
  }

  return prefs->GetInteger(prefs::kGeminiSparkSettings) ==
         static_cast<int>(gemini::SparkSettingsPolicy::kAllowed);
}
