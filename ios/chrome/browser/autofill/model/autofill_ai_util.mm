// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"

#import "base/strings/string_util.h"
#import "components/account_settings/account_setting_service.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "components/autofill/core/browser/integrators/personal_context/personal_context_autofill_util.h"
#import "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_eligibility_service.h"
#import "components/personal_context/core/personal_context_types.h"
#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "components/subscription_eligibility/subscription_eligibility_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/account_settings/model/ios_account_setting_service_factory.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_eligibility_service_factory.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_first_run_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/subscription_eligibility/model/subscription_eligibility_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Returns true if the user has completed the Gemini First Run Experience.
bool IsGeminiFirstRunCompleted(ProfileIOS* profile) {
  return gemini::CurrentFirstRunState(profile->GetPrefs()) ==
         gemini::FirstRunState::kCompleted;
}

}  // namespace

namespace autofill {

using personal_context::PersonalContextEligibilityService;
using personal_context::PersonalContextEligibilityState;

const std::string GetCountryCodeFromVariations() {
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  return variations_service
             ? base::ToUpperASCII(variations_service->GetLatestCountry())
             : std::string();
}

bool IsWalletPublicPassStorageEnabled(ProfileIOS* profile) {
  account_settings::AccountSettingService* setting_service =
      IOSAccountSettingServiceFactory::GetForProfile(profile);
  return setting_service &&
         setting_service
             ->GetBoolean(account_settings::kWalletPrivacyContextualSurfacing)
             .value_or(false);
}

bool CanPerformAutofillAiAction(ProfileIOS* profile,
                                AutofillAiAction action,
                                std::optional<EntityType> entity_type) {
  EntityDataManager* entity_data_manager =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile);
  if (!entity_data_manager) {
    return false;
  }

  // `MayPerformAutofillAiAction` has two overloads:
  // - One that takes a reference to `AutofillClient`, an action and two other
  //   parameters.
  // - One that takes 8 separate parameters, an action and two other
  //   parameters.
  //
  // The latter is used here since Settings, where this function is being used,
  // does not have an associated `WebState`, and therefore, does not have an
  // `AutofillClient`.
  // IdentityManagerFactory, GoogleGroupsManagerFactory and SyncServiceFactory
  // require original profile. Here SyncServiceFactory uses the profile directly
  // just as ChromeAutofillClientIOS does.
  //
  // For Incognito profiles, it is allowed to fill only based on cached
  // predictions. It is also allowed to manage data. Although it is not relevant
  // for Bling since the management of data is done in Settings, which is not
  // in Incognito. No save. No model inference on forms. Therefore, there will
  // be no sync service if the profile is an Incognito profile.

  PersonalContextEligibilityService* personal_context_eligibility_service =
      IOSPersonalContextEligibilityServiceFactory::GetForProfile(profile);
  const PersonalContextEligibilityState personal_context_eligibility_state =
      personal_context_eligibility_service
          ? personal_context_eligibility_service->GetEligibilityState()
          : PersonalContextEligibilityState::kDisabledNotEligible;

  return MayPerformAutofillAiAction(
      GoogleGroupsManagerFactory::GetForProfile(profile->GetOriginalProfile()),
      profile->GetPrefs(), entity_data_manager,
      IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile()),
      SyncServiceFactory::GetForProfile(profile),
      IsWalletPublicPassStorageEnabled(profile), profile->IsOffTheRecord(),
      GeoIpCountryCode(GetCountryCodeFromVariations()),
      SubscriptionEligibilityServiceFactory::GetForProfile(profile),
      personal_context_eligibility_state, action, entity_type);
}

bool IsAmbientAutofillEnabled(ProfileIOS* profile) {
  CHECK(profile);
  if (profile->IsOffTheRecord()) {
    return false;
  }

  // Check Gemini First Run state before any pcontext features.
  if (!IsGeminiFirstRunCompleted(profile)) {
    return false;
  }

  return CanPerformAutofillAiAction(profile,
                                    AutofillAiAction::kAmbientAutofill);
}

bool IsAmbientAutofillFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kAutofillAmbientAutofill);
}

bool IsAutofillShoppingEnabled() {
  return IsAmbientAutofillFeatureEnabled() ||
         base::FeatureList::IsEnabled(features::kAutofillAiWalletShopping);
}

bool IsAutofillAtMemoryEnabled() {
  return base::FeatureList::IsEnabled(features::kAutofillAtMemory);
}

bool IsEnhancedAutofillEnabled(ProfileIOS* profile) {
  ProfileIOS* original_profile = profile->GetOriginalProfile();
  return GetAutofillAiOptInStatus(
      original_profile->GetPrefs(),
      IdentityManagerFactory::GetForProfile(original_profile));
}

void SetEnhancedAutofillEnabled(ProfileIOS* profile, bool enabled) {
  ProfileIOS* original_profile = profile->GetOriginalProfile();
  SetAutofillAiOptInStatus(
      GoogleGroupsManagerFactory::GetForProfile(original_profile),
      original_profile->GetPrefs(),
      IOSAutofillEntityDataManagerFactory::GetForProfile(original_profile),
      IdentityManagerFactory::GetForProfile(original_profile),
      SyncServiceFactory::GetForProfile(original_profile),
      IsWalletPublicPassStorageEnabled(original_profile),
      original_profile->IsOffTheRecord(),
      GeoIpCountryCode(GetCountryCodeFromVariations()),
      SubscriptionEligibilityServiceFactory::GetForProfile(original_profile),
      PersonalContextEligibilityState::kDisabledNotEligible,
      enabled ? AutofillAiOptInStatus::kOptedIn
              : AutofillAiOptInStatus::kOptedOut);
}

base::optional_ref<const EntityInstance> GetEntityInstance(
    ProfileIOS* profile,
    const Suggestion::Payload& payload) {
  if (!profile) {
    return std::nullopt;
  }

  if (!std::holds_alternative<Suggestion::AutofillAiPayload>(payload)) {
    return std::nullopt;
  }

  const std::string guid =
      std::get<Suggestion::AutofillAiPayload>(payload).guid.value();

  if (guid.empty()) {
    return std::nullopt;
  }

  EntityDataManager* edm =
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile);
  if (!edm) {
    return std::nullopt;
  }

  return edm->GetEntityInstance(
      EntityInstance::EntityId(base::Uuid::ParseCaseInsensitive(guid)));
}

bool ShouldShowPersonalContextAutofillSetting(ProfileIOS* profile) {
  CHECK(profile);
  return ShouldShowPersonalContextAutofillSetting(
      GoogleGroupsManagerFactory::GetForProfile(profile->GetOriginalProfile()),
      profile->GetPrefs(),
      IOSAutofillEntityDataManagerFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile()),
      SyncServiceFactory::GetForProfile(profile),
      IsWalletPublicPassStorageEnabled(profile), profile->IsOffTheRecord(),
      GeoIpCountryCode(GetCountryCodeFromVariations()),
      IOSPersonalContextEligibilityServiceFactory::GetForProfile(profile),
      SubscriptionEligibilityServiceFactory::GetForProfile(profile));
}

}  // namespace autofill
