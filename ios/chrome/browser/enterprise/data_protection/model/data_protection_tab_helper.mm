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

}  // namespace

DataProtectionTabHelper::DataProtectionTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_);
  web_state->AddObserver(this);
  // Check the currently visible URL
  CheckPolicyForInitialURL();
}

DataProtectionTabHelper::~DataProtectionTabHelper() = default;

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
  pending_navigation_ = {.navigation_id = GetNavigationId(navigation_context)};

  PerformChecks(navigation_context);
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
  if (pending_navigation_.screenshot_protection_enabled) {
    return;
  }

  PerformChecks(navigation_context);
}

void DataProtectionTabHelper::CheckPolicyForInitialURL() {
  const GURL& url = web_state_->GetVisibleURL();
  if (SkipUrl(url)) {
    return;
  }

  committed_navigation_.navigation_id = std::nullopt;
  if (IsScreenshotBlockedByDataControls(url)) {
    UpdateScreenshotProtectionState(true);
  } else {
    EvaluateRealTimePolicy(url, committed_navigation_.navigation_id);
  }
}

void DataProtectionTabHelper::PerformChecks(
    web::NavigationContext* navigation_context) {
  CHECK(!pending_navigation_.screenshot_protection_enabled);
  if (SkipUrl(navigation_context->GetUrl())) {
    return;
  }

  pending_navigation_.screenshot_protection_enabled =
      IsScreenshotBlockedByDataControls(navigation_context->GetUrl());

  if (!pending_navigation_.screenshot_protection_enabled) {
    EvaluateRealTimePolicy(navigation_context->GetUrl(),
                           pending_navigation_.navigation_id);
  }
}

bool DataProtectionTabHelper::IsScreenshotBlockedByDataControls(
    const GURL& url) {
  CHECK(GetRulesService());
  return GetRulesService()->BlockScreenshots(url);
}

bool DataProtectionTabHelper::ShouldPerformRealTimeLookup() const {
  ProfileIOS* profile = GetProfile();
  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }

  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForProfile(profile);

  return connectors_service &&
         connectors_service->GetDMTokenForRealTimeUrlCheck().has_value() &&
         GetRealTimeLookupService() && GetLookupService();
}

void DataProtectionTabHelper::EvaluateRealTimePolicy(
    const GURL& url,
    std::optional<int64_t> navigation_id) {
  if (!ShouldPerformRealTimeLookup()) {
    return;
  }

  GetLookupService()->DoLookup(
      GetRealTimeLookupService(), url,
      base::BindOnce(&DataProtectionTabHelper::OnRealTimeLookupResult,
                     weak_factory_.GetWeakPtr(), navigation_id),
      web_state_->GetUniqueIdentifier().ToSessionID());
}

void DataProtectionTabHelper::OnRealTimeLookupResult(
    std::optional<int64_t> navigation_id,
    std::unique_ptr<safe_browsing::RTLookupResponse> response) {
  if (!response) {
    return;
  }

  enterprise_data_protection::UrlSettings settings =
      enterprise_data_protection::GetUrlSettings(std::string(), response.get());

  if (settings.allow_screenshots) {
    return;  // No need to do anything if not blocked.
  }

  // If this result is for the currently pending navigation, latch the
  // protection state.
  if (navigation_id == pending_navigation_.navigation_id) {
    pending_navigation_.screenshot_protection_enabled = true;
  }

  // If this result is for the *already committed* navigation (e.g. an
  // asynchronous lookup finished after commit), update the live state
  // immediately.
  if (navigation_id == committed_navigation_.navigation_id) {
    UpdateScreenshotProtectionState(true);
  }
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
    UpdateScreenshotProtectionState(
        pending_navigation_.screenshot_protection_enabled);
  }

  // Reset pending state now that the navigation has finished.
  pending_navigation_ = {};
}

void DataProtectionTabHelper::UpdateScreenshotProtectionState(bool new_state) {
  if (committed_navigation_.screenshot_protection_enabled == new_state) {
    return;
  }
  committed_navigation_.screenshot_protection_enabled = new_state;
  for (auto& observer : observers_) {
    observer.ScreenshotProtectionDidChange(
        web_state_, committed_navigation_.screenshot_protection_enabled);
  }
}

void DataProtectionTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  pending_navigation_ = {};
  committed_navigation_ = {};
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
