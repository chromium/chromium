// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_availability.h"

#import "base/strings/string_util.h"
#import "components/country_codes/country_codes.h"
#import "components/prefs/pref_service.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
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
#import "ios/web/public/web_state.h"

namespace {

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
    case EntryPoint::Toolbar: {
      // The Toolbar Gemini button is not visible or enabled in EEA countries or
      // Japan.
      if (IsInEEAOrJapan()) {
        result.visible = false;
        result.enabled = false;
        break;
      }

      if (profile_eligible) {
        result.visible = true;
      } else if (gemini_service && auth_service &&
                 !auth_service->HasPrimaryIdentity()) {
        // If the profile is ineligible because the user is signed out, show the
        // Gemini button and keep it enabled so tapping it triggers the sign-in
        // flow, unless a local enterprise policy explicitly disables it.
        result.visible = gemini::GeminiAllowedByPolicy(pref_service);
      }

      result.enabled = result.visible && web_state_eligible;
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

    case EntryPoint::ImageContextMenu:
    case EntryPoint::ImageRemixIPH: {
      bool eligible = profile_eligible && web_state_eligible &&
                      IsFeatureAvailable(Feature::kImageRemix, profile) &&
                      tab_helper->IsContextualEntryPointAllowed();
      result.visible = eligible;
      result.enabled = eligible;
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
