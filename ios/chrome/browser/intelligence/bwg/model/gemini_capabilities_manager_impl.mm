// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_impl.h"

#import "base/check.h"
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
  CHECK(IsAppSwitcherAISummarizationEnabled());

  NSUserDefaults* shared_defaults = app_group::GetCommonGroupUserDefaults();
  NSDictionary* existing_capabilities = [shared_defaults
      dictionaryForKey:app_group::kChromeCapabilitiesPreference];
  NSMutableDictionary* capabilities = existing_capabilities
                                          ? [existing_capabilities mutableCopy]
                                          : [[NSMutableDictionary alloc] init];

  bool has_primary_identity =
      authentication_service_ && authentication_service_->HasPrimaryIdentity();
  bool user_eligible =
      gemini_service_ && gemini_service_->IsProfileEligibleForGemini();
  UpdateSupportsAISummarization(capabilities);
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
