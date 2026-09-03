// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_availability.h"

#import "base/check.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/country_codes/country_codes.h"
#import "components/prefs/pref_service.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/web_state.h"
#import "net/base/network_change_notifier.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Date template for the refill reset time (month, day, and time without year).
NSString* const kLimitResetDateFormatTemplate = @"MMMdjm";

// Returns true if the device's stored permanent country is in the EEA or
// Japan.
bool IsInEEAOrJapan() {
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  if (!variations_service) {
    return false;
  }
  country_codes::CountryId country_id(
      base::ToUpperASCII(variations_service->GetStoredPermanentCountry()));
  return regional_capabilities::RegionalCapabilitiesService::
             IsInAnySearchEngineChoiceScreenRegion(country_id) ||
         country_id == country_codes::CountryId("JP");
}

// Returns true if the primary identity is in an unverified state requiring
// re-authentication / managing account approval.
bool IsPrimaryIdentityUnverified(AuthenticationService* auth_service,
                                 signin::IdentityManager* identity_manager) {
  if (!auth_service || !auth_service->HasPrimaryIdentity() ||
      !identity_manager) {
    return false;
  }
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    return false;
  }
  return identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
      account_id);
}

// Returns whether Gemini is eligible for bar surfaces (Toolbar and AppBar).
bool IsGeminiEligibleForBar(ProfileIOS* profile,
                            GeminiService* gemini_service,
                            AuthenticationService* auth_service,
                            PrefService* pref_service) {
  if (!IsPageActionMenuEnabled() || IsInEEAOrJapan() || !gemini_service) {
    return false;
  }

  if (pref_service && !gemini::GeminiAllowedByPolicy(pref_service)) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      profile ? IdentityManagerFactory::GetForProfile(profile) : nullptr;
  bool is_signed_out = !auth_service || !auth_service->HasPrimaryIdentity();
  bool is_unverified =
      IsPrimaryIdentityUnverified(auth_service, identity_manager);
  if (is_signed_out || is_unverified) {
    // For signed-out or unverified users, optimistically show the button to
    // encourage sign-in or verification if sign-in is allowed.
    return auth_service && auth_service->SigninEnabled();
  }

  if (!net::NetworkChangeNotifier::IsOffline() &&
      !gemini_service->IsWorkspacePolicyCheckPending()) {
    std::optional<gemini::IneligibilityReasons> ineligibility_reasons =
        gemini_service->GeminiIneligibilityForProfile();
    if (ineligibility_reasons && ineligibility_reasons->workspace) {
      return false;
    }
  }

  return true;
}

// Returns a localized string describing when the image remix limit resets.
NSString* GetImageRemixLimitResetSubtitle() {
  NSDate* refill_date = ios::provider::GetRefillDateForFeatureMode(
      ios::provider::GeminiFeatureMode::kNanoBanana);
  CHECK(refill_date);
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setLocalizedDateFormatFromTemplate:kLimitResetDateFormatTemplate];
  NSString* formatted_date = [formatter stringFromDate:refill_date];
  return l10n_util::GetNSStringF(
      IDS_IOS_GEMINI_IMAGE_REMIX_LIMIT_RESET_SUBTITLE,
      base::SysNSStringToUTF16(formatted_date));
}

// Returns whether Image Remix is eligible for the profile and web state.
bool IsImageRemixEligible(bool profile_eligible,
                          bool web_state_eligible,
                          ProfileIOS* profile,
                          GeminiTabHelper* tab_helper) {
  return profile_eligible && web_state_eligible &&
         gemini::IsFeatureAvailable(gemini::Feature::kImageRemix, profile) &&
         tab_helper && tab_helper->IsContextualEntryPointAllowed();
}

// Returns whether the user's Image Remix quota has been reached.
bool IsImageRemixQuotaReached() {
  return IsGeminiAureusEnabled() &&
         ios::provider::IsFeatureModeDisabledByQuota(
             ios::provider::GeminiFeatureMode::kNanoBanana);
}

}  // namespace

namespace gemini {

GeminiAvailabilityResult IsGeminiAvailable(EntryPoint entry_point,
                                           ProfileIOS* profile,
                                           web::WebState* web_state,
                                           AuthenticationService* auth_service,
                                           PrefService* pref_service) {
  GeminiAvailabilityResult result;

  // High level flag check which verifies country/locale along with kill switch
  // checking.
  if (!IsPageActionMenuEnabled()) {
    return result;
  }

  // Account/profile based eligibility checks via GeminiService.
  GeminiService* gemini_service = GeminiServiceFactory::GetForProfile(profile);
  bool profile_eligible =
      gemini_service && gemini_service->IsProfileEligibleForGemini();
  if (gemini_service) {
    result.ineligibility_reasons =
        gemini_service->GeminiIneligibilityForProfile();
  }
  GeminiTabHelper* tab_helper =
      web_state ? GeminiTabHelper::FromWebState(web_state) : nullptr;

  // Web state eligibility check via GeminiTabHelper.
  bool web_state_eligible =
      tab_helper && tab_helper->IsGeminiAvailableForWebState();

  switch (entry_point) {
    case EntryPoint::Toolbar:
    case EntryPoint::AppBar: {
      if (!profile && web_state) {
        profile = ProfileIOS::FromBrowserState(web_state->GetBrowserState());
      }
      if (!profile) {
        result.visible = false;
        result.enabled = false;
        break;
      }
      if (!gemini_service) {
        gemini_service = GeminiServiceFactory::GetForProfile(profile);
        if (gemini_service) {
          result.ineligibility_reasons =
              gemini_service->GeminiIneligibilityForProfile();
        }
      }
      if (!auth_service) {
        auth_service = AuthenticationServiceFactory::GetForProfile(profile);
      }
      if (!pref_service) {
        pref_service = profile->GetPrefs();
      }
      bool eligible = IsGeminiEligibleForBar(profile, gemini_service,
                                             auth_service, pref_service);
      result.visible = eligible;
      result.enabled = eligible;
      break;
    }

    case EntryPoint::AppSwitcherAISummarization: {
      // AppSwitcher operates across tabs/apps in the background and does not
      // evaluate a single active web state so we do not check the web state
      // eligibility.
      bool eligible = IsAppSwitcherAISummarizationEnabled() && profile_eligible;
      result.visible = eligible;
      result.enabled = eligible;
      break;
    }

    case EntryPoint::AtMemorySearch: {
      // AtMemorySearch operates over stored user memories and does not evaluate
      // active web state eligibility.
      result.visible = profile_eligible;
      result.enabled = profile_eligible;
      break;
    }

    case EntryPoint::ImageContextMenu: {
      bool eligible = IsImageRemixEligible(profile_eligible, web_state_eligible,
                                           profile, tab_helper);
      bool quota_reached = eligible && IsImageRemixQuotaReached();
      // This entry point can be visible even if the feature is disabled.
      result.visible = eligible;
      result.enabled = eligible && !quota_reached;
      if (quota_reached) {
        result.disabled_reason =
            gemini::EntryPointDisabledReason::kQuotaExhausted;
        result.disabled_reason_subtitle = GetImageRemixLimitResetSubtitle();
      }
      break;
    }

    case EntryPoint::ImageRemixIPH: {
      bool eligible = IsImageRemixEligible(profile_eligible, web_state_eligible,
                                           profile, tab_helper);
      bool quota_reached = eligible && IsImageRemixQuotaReached();
      // IPH must not be shown if the feature is disabled.
      result.visible = eligible && !quota_reached;
      result.enabled = eligible && !quota_reached;
      if (quota_reached) {
        result.disabled_reason =
            gemini::EntryPointDisabledReason::kQuotaExhausted;
        result.disabled_reason_subtitle = GetImageRemixLimitResetSubtitle();
      }
      break;
    }

    case EntryPoint::EditMenu: {
      bool eligible = profile_eligible && web_state_eligible &&
                      ExplainGeminiEditMenuPosition() !=
                          PositionForExplainGeminiEditMenu::kDisabled &&
                      tab_helper->IsContextualEntryPointAllowed();
      result.visible = eligible;
      result.enabled = eligible;
      break;
    }

    default: {
      bool eligible = profile_eligible && web_state_eligible;
      result.visible = eligible;
      result.enabled = eligible;
      break;
    }
  }

  return result;
}

}  // namespace gemini
