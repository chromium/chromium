// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_impl.h"

#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_availability.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

GeminiCapabilitiesManagerImpl::GeminiCapabilitiesManagerImpl(
    ProfileIOS* profile,
    AuthenticationService* authentication_service,
    GeminiService* gemini_service)
    : profile_(profile),
      authentication_service_(authentication_service),
      gemini_service_(gemini_service) {
  if (gemini_service_) {
    gemini_service_observation_.Observe(gemini_service_);
  }
  // Update capabilities immediately upon initialization.
  UpdateCapabilities();
}

GeminiCapabilitiesManagerImpl::~GeminiCapabilitiesManagerImpl() = default;

void GeminiCapabilitiesManagerImpl::Shutdown() {
  gemini_service_observation_.Reset();
  authentication_service_ = nullptr;
  gemini_service_ = nullptr;
}

void GeminiCapabilitiesManagerImpl::OnGeminiEligibilityChanged() {
  UpdateCapabilities();
}

void GeminiCapabilitiesManagerImpl::UpdateCapabilities() {
  NSUserDefaults* shared_defaults = app_group::GetCommonGroupUserDefaults();

  // If the feature is disabled, clean up all capabilities and return early.
  if (!IsAppSwitcherAISummarizationEnabled()) {
    UpdateHashedUserID(shared_defaults, /*has_primary_identity=*/false);

    NSDictionary* existing_capabilities = [shared_defaults
        dictionaryForKey:app_group::kChromeCapabilitiesPreference];
    if (existing_capabilities) {
      NSMutableDictionary* capabilities = [existing_capabilities mutableCopy];
      [capabilities removeObjectForKey:
                        app_group::kChromeSupportsAISummarizationCapability];
      [capabilities removeObjectForKey:
                        app_group::kChromeUserIsEligibleForGeminiCapability];
      if (![existing_capabilities isEqualToDictionary:capabilities]) {
        [shared_defaults setObject:capabilities
                            forKey:app_group::kChromeCapabilitiesPreference];
      }
    }
    return;
  }

  NSDictionary* existing_capabilities = [shared_defaults
      dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  NSMutableDictionary* capabilities = existing_capabilities
                                          ? [existing_capabilities mutableCopy]
                                          : [NSMutableDictionary dictionary];

  bool has_primary_identity =
      authentication_service_ && authentication_service_->HasPrimaryIdentity();
  bool user_eligible =
      gemini_service_ && gemini_service_->IsProfileEligibleForGemini();
  UpdateSupportsAISummarization(capabilities);
  UpdateHashedUserID(shared_defaults, has_primary_identity);
  UpdateUserEligibility(capabilities, user_eligible, has_primary_identity);

  if (![existing_capabilities isEqualToDictionary:capabilities]) {
    [shared_defaults setObject:capabilities
                        forKey:app_group::kChromeCapabilitiesPreference];
  }
}

#pragma mark - Private

void GeminiCapabilitiesManagerImpl::UpdateSupportsAISummarization(
    NSMutableDictionary* capabilities) {
  capabilities[app_group::kChromeSupportsAISummarizationCapability] =
      @(IsAppSwitcherAISummarizationEnabled());
}

void GeminiCapabilitiesManagerImpl::UpdateHashedUserID(
    NSUserDefaults* shared_defaults,
    bool has_primary_identity) {
  if (!has_primary_identity) {
    if ([shared_defaults objectForKey:app_group::kAppSwitcherHashedUserID]) {
      [shared_defaults removeObjectForKey:app_group::kAppSwitcherHashedUserID];
    }
    return;
  }
  id<SystemIdentity> identity = authentication_service_->GetPrimaryIdentity();
  NSString* existing_hashed_uid =
      [shared_defaults stringForKey:app_group::kAppSwitcherHashedUserID];
  if (![existing_hashed_uid isEqualToString:identity.hashedGaiaID]) {
    [shared_defaults setObject:identity.hashedGaiaID
                        forKey:app_group::kAppSwitcherHashedUserID];
  }
}

void GeminiCapabilitiesManagerImpl::UpdateUserEligibility(
    NSMutableDictionary* capabilities,
    bool user_eligible,
    bool has_primary_identity) {
  bool eligible = false;
  if (has_primary_identity && profile_) {
    eligible =
        gemini::IsGeminiAvailable(
            gemini::EntryPoint::AppSwitcherAISummarization, profile_, nullptr)
            .enabled;
  }

  capabilities[app_group::kChromeUserIsEligibleForGeminiCapability] =
      @(eligible);
}
