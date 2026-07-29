// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "components/content_settings/core/browser/content_settings_utils.h"
#import "components/enterprise/data_protection/data_protection_url_lookup_service.h"
#import "components/enterprise/data_protection/utils.h"
#import "components/safe_browsing/core/browser/realtime/chrome_enterprise_url_lookup_service.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#import "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_url_lookup_service_factory.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/safe_browsing/model/chrome_enterprise_url_lookup_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

using ProtectionState = DataProtectionTabHelper::ProtectionState;
using Enabled = DataProtectionTabHelper::Enabled;
using Disabled = DataProtectionTabHelper::Disabled;
using LookupPending = DataProtectionTabHelper::LookupPending;
using ScreenshotBlockSource = DataProtectionTabHelper::ScreenshotBlockSource;

// Returns the navigation ID associated with the given `context`, or 0 if the
// context is null.
int64_t GetNavigationId(web::NavigationContext* context) {
  return context ? context->GetNavigationId() : 0;
}

// Returns true if data protection checks should be skipped for the given URL.
bool SkipUrl(const GURL& url) {
  return !url.is_valid() || UrlHasChromeScheme(url) || IsUrlNtp(url) ||
         url.SchemeIs(content_settings::kChromeUIUntrustedScheme) ||
         net::IsLocalhost(url);
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

void DataProtectionTabHelper::WasShown(web::WebState* web_state) {
  for (auto& observer : observers_) {
    observer.OnWatermarkViewNeedsUpdate(web_state);
  }
}

void DataProtectionTabHelper::WasHidden(web::WebState* web_state) {
  for (auto& observer : observers_) {
    observer.OnWatermarkViewNeedsUpdate(web_state);
  }
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
  // for all subsequent redirects. No further checks are needed unless
  // watermarking feature is enabled.
  if (std::holds_alternative<Enabled>(pending_navigation_.protection_state) &&
      !IsEnableEnterpriseWatermarkingIOS()) {
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
    SetCommittedProtectionState(pending_navigation_.protection_state,
                                pending_navigation_.watermark_text);
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
  // If protection is already explicitly enabled, no further checks are needed
  // unless enterprise watermarking is enabled.
  if (!IsEnableEnterpriseWatermarkingIOS()) {
    CHECK(!std::holds_alternative<Enabled>(navigation.protection_state));
  }

  navigation.watermark_text.clear();
  if (SkipUrl(url)) {
    // Force a committed state update if we are checking the committed
    // navigation.
    if (&navigation == &committed_navigation_) {
      SetCommittedProtectionState(Disabled{}, "");
    }
    return;
  }

  if (GetRulesService()->BlockScreenshots(url)) {
    base::UmaHistogramEnumeration(kScreenshotBlockSourceHistogram,
                                  ScreenshotBlockSource::kDataControls);
    SetProtectionState(navigation, ProtectionState(Enabled{}), "");

    // If watermarking is enabled, we still need to perform the real-time lookup
    // to fetch the watermark text, even if screenshot protection is already
    // enabled by a Data Controls policy.
    if (!IsEnableEnterpriseWatermarkingIOS()) {
      return;
    }
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

  SetProtectionState(navigation,
                     ComputeNextStateOnLookupStart(navigation.protection_state),
                     "");

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
  std::string watermark_text;
  std::string identifier;
  base::UmaHistogramBoolean(kScreenshotBlockLookupSuccessHistogram,
                            response != nullptr);

  enterprise_connectors::ConnectorsService* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForProfile(
          GetProfile());
  if (connectors_service) {
    identifier = connectors_service->GetRealTimeUrlCheckIdentifier();
  }

  if (response) {
    enterprise_data_protection::UrlSettings settings =
        enterprise_data_protection::GetUrlSettings(identifier, response.get());
    protection_enabled = !settings.allow_screenshots;
    watermark_text =
        IsEnableEnterpriseWatermarkingIOS() ? settings.watermark_text : "";

    // TODO(crbug.com/538614118): Add histogram for watermarking.
    if (protection_enabled) {
      base::UmaHistogramEnumeration(kScreenshotBlockSourceHistogram,
                                    ScreenshotBlockSource::kRealtimeLookup);
    }
  }

  if (navigation_id == pending_navigation_.navigation_id) {
    pending_navigation_.protection_state = ComputeNextStateOnLookupResponse(
        pending_navigation_.protection_state, protection_enabled);
    pending_navigation_.watermark_text = watermark_text;
  }

  if (navigation_id == committed_navigation_.navigation_id) {
    SetCommittedProtectionState(
        ComputeNextStateOnLookupResponse(committed_navigation_.protection_state,
                                         protection_enabled),
        watermark_text);
  }
}

void DataProtectionTabHelper::SetProtectionState(
    NavigationState& navigation,
    ProtectionState state,
    const std::string& watermark_text) {
  if (&navigation == &committed_navigation_) {
    SetCommittedProtectionState(state, watermark_text);
  } else {
    navigation.protection_state = state;
    navigation.watermark_text = watermark_text;
  }
}

void DataProtectionTabHelper::SetCommittedProtectionState(
    ProtectionState new_state,
    const std::string& watermark_text) {
  bool previous_screenshot_protection = IsScreenshotProtectionEnabled();
  std::string previous_watermark = GetWatermarkText();

  committed_navigation_.protection_state = new_state;
  committed_navigation_.watermark_text = watermark_text;

  if (previous_screenshot_protection != IsScreenshotProtectionEnabled()) {
    for (auto& observer : observers_) {
      observer.ScreenshotProtectionDidChange(web_state_,
                                             IsScreenshotProtectionEnabled());
    }
  }

  if (previous_watermark != watermark_text) {
    for (auto& observer : observers_) {
      observer.WatermarkTextDidChange(web_state_, watermark_text);
    }

    // TODO(crbug.com/533013176): Record the webstate_id to the prefs map
    // kDataProtectionWatermarkedTabs for tab-grid.
  }
}

void DataProtectionTabHelper::DidStopLoading(web::WebState* web_state) {
  for (auto& observer : observers_) {
    observer.OnWatermarkViewNeedsUpdate(web_state);
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
  return safe_browsing::ChromeEnterpriseRealTimeUrlLookupServiceFactory::
      GetForProfile(GetProfile());
}
