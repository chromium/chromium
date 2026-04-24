// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"

#import "base/functional/bind.h"
#import "components/content_settings/core/browser/content_settings_utils.h"
#import "components/enterprise/data_protection/data_protection_url_lookup_service.h"
#import "components/enterprise/data_protection/utils.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#import "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_url_lookup_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/real_time_url_lookup_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {

using ProtectionState = DataProtectionTabHelper::ProtectionState;
using Enabled = DataProtectionTabHelper::Enabled;
using Disabled = DataProtectionTabHelper::Disabled;
using LookupPending = DataProtectionTabHelper::LookupPending;

// Returns the navigation ID associated with the given `context`, or 0 if the
// context is null.
int64_t GetNavigationId(web::NavigationContext* context) {
  return context ? context->GetNavigationId() : 0;
}

// Returns true if data protection checks should be skipped for the given URL.
bool SkipUrl(const GURL& url) {
  return !url.is_valid() || url.SchemeIs(kChromeUIScheme) ||
         url.SchemeIs(content_settings::kChromeUIUntrustedScheme);
}

// Returns the next state when a new real-time lookup is initiated.
ProtectionState ComputeNextStateOnLookupStart(
    const ProtectionState& current_state) {
  if (std::holds_alternative<Enabled>(current_state)) {
    return current_state;
  }
  if (const auto* pending = std::get_if<LookupPending>(&current_state)) {
    return ProtectionState(
        LookupPending{.pending_count = pending->pending_count + 1});
  }
  return ProtectionState(LookupPending{.pending_count = 1});
}

// Returns the next state based on the current state and the result of a
// real-time lookup.
ProtectionState ComputeNextStateOnLookupResponse(
    const ProtectionState& current_state,
    bool protection_enabled) {
  if (protection_enabled) {
    return ProtectionState(Enabled{});
  }

  if (const auto* pending = std::get_if<LookupPending>(&current_state)) {
    if (pending->pending_count > 1) {
      return ProtectionState(
          LookupPending{.pending_count = pending->pending_count - 1});
    }
    return ProtectionState(Disabled{});
  }

  return current_state;
}

}  // namespace

DataProtectionTabHelper::DataProtectionTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_);
  web_state->AddObserver(this);
  // Check the currently visible URL
  CheckPolicyForInitialURL();
}

DataProtectionTabHelper::~DataProtectionTabHelper() {
  for (auto& observer : observers_) {
    observer.DataProtectionTabHelperDestroyed(this);
  }
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

DataProtectionTabHelper::NavigationState::NavigationState(
    std::optional<int64_t> navigation_id)
    : navigation_id(navigation_id) {}
DataProtectionTabHelper::NavigationState::~NavigationState() = default;

void DataProtectionTabHelper::AddObserver(
    DataProtectionTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void DataProtectionTabHelper::RemoveObserver(
    DataProtectionTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DataProtectionTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context) {
    return;
  }

  // Reset the pending navigation state for this new navigation.
  pending_navigation_ = NavigationState(GetNavigationId(navigation_context));

  PerformChecks(navigation_context->GetUrl(), pending_navigation_);
}

void DataProtectionTabHelper::DidRedirectNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context || GetNavigationId(navigation_context) !=
                                 pending_navigation_.navigation_id) {
    return;
  }

  // Once screenshot protection is enabled for a navigation, it stays enabled
  // for all subsequent redirects. No further checks are needed.
  if (std::holds_alternative<Enabled>(pending_navigation_.protection_state)) {
    return;
  }

  PerformChecks(navigation_context->GetUrl(), pending_navigation_);
}

void DataProtectionTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context) {
    return;
  }

  int64_t nav_id = GetNavigationId(navigation_context);
  if (nav_id != pending_navigation_.navigation_id) {
    return;  // Not the navigation we were tracking as pending.
  }

  // If the navigation successfully committed, propagate the pending protection
  // state to the committed state.
  if (navigation_context->HasCommitted()) {
    // This navigation is now committed.
    committed_navigation_.navigation_id = nav_id;
    SetCommittedProtectionState(pending_navigation_.protection_state);
  }

  // Reset pending state now that the navigation has finished.
  pending_navigation_ = NavigationState();
}

void DataProtectionTabHelper::CheckPolicyForInitialURL() {
  const GURL& url = web_state_->GetVisibleURL();
  PerformChecks(url, committed_navigation_);
}

void DataProtectionTabHelper::PerformChecks(const GURL& url,
                                            NavigationState& navigation) {
  // If protection is already explicitly enabled, no further checks are needed.
  CHECK(!std::holds_alternative<Enabled>(navigation.protection_state));

  if (SkipUrl(url)) {
    return;
  }

  if (GetRulesService()->BlockScreenshots(url)) {
    SetProtectionState(navigation, ProtectionState(Enabled{}));
    return;
  }

  EvaluateRealTimePolicy(url, navigation);
}

bool DataProtectionTabHelper::ShouldPerformRealTimeLookup() const {
  ProfileIOS* profile = GetProfile();
  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }

  enterprise_connectors::ConnectorsService* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForProfile(profile);

  return connectors_service &&
         connectors_service->GetDMTokenForRealTimeUrlCheck().has_value() &&
         GetRealTimeLookupService() && GetLookupService();
}

void DataProtectionTabHelper::EvaluateRealTimePolicy(
    const GURL& url,
    NavigationState& navigation) {
  if (!ShouldPerformRealTimeLookup()) {
    return;
  }

  SetProtectionState(
      navigation, ComputeNextStateOnLookupStart(navigation.protection_state));

  GetLookupService()->DoLookup(
      GetRealTimeLookupService(), url,
      base::BindOnce(&DataProtectionTabHelper::OnRealTimeLookupResult,
                     weak_factory_.GetWeakPtr(), navigation.navigation_id),
      web_state_->GetUniqueIdentifier().ToSessionID());
}

void DataProtectionTabHelper::OnRealTimeLookupResult(
    std::optional<int64_t> navigation_id,
    std::unique_ptr<safe_browsing::RTLookupResponse> response) {
  // If the lookup failed, we default to the enabled state (fail-closed).
  bool protection_enabled = true;
  if (response) {
    enterprise_data_protection::UrlSettings settings =
        enterprise_data_protection::GetUrlSettings(std::string(),
                                                   response.get());
    protection_enabled = !settings.allow_screenshots;
  }

  if (navigation_id == pending_navigation_.navigation_id) {
    pending_navigation_.protection_state = ComputeNextStateOnLookupResponse(
        pending_navigation_.protection_state, protection_enabled);
  }

  if (navigation_id == committed_navigation_.navigation_id) {
    SetCommittedProtectionState(ComputeNextStateOnLookupResponse(
        committed_navigation_.protection_state, protection_enabled));
  }
}

void DataProtectionTabHelper::SetProtectionState(NavigationState& navigation,
                                                 ProtectionState state) {
  if (&navigation == &committed_navigation_) {
    SetCommittedProtectionState(state);
  } else {
    navigation.protection_state = state;
  }
}

void DataProtectionTabHelper::SetCommittedProtectionState(
    ProtectionState new_state) {
  bool old_value = IsScreenshotProtectionEnabled();
  committed_navigation_.protection_state = new_state;

  if (old_value == IsScreenshotProtectionEnabled()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.ScreenshotProtectionDidChange(web_state_,
                                           IsScreenshotProtectionEnabled());
  }
}

void DataProtectionTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  pending_navigation_ = NavigationState();
  committed_navigation_ = NavigationState();
}

ProfileIOS* DataProtectionTabHelper::GetProfile() const {
  return ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
}

data_controls::IOSRulesService* DataProtectionTabHelper::GetRulesService()
    const {
  return data_controls::IOSRulesServiceFactory::GetForProfile(GetProfile());
}

enterprise_data_protection::DataProtectionUrlLookupService*
DataProtectionTabHelper::GetLookupService() const {
  return DataProtectionUrlLookupServiceFactory::GetForProfile(GetProfile());
}

safe_browsing::RealTimeUrlLookupServiceBase*
DataProtectionTabHelper::GetRealTimeLookupService() const {
  return RealTimeUrlLookupServiceFactory::GetForProfile(GetProfile());
}
