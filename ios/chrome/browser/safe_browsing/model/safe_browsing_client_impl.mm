// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_client_impl.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/core/common/utils.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/prerender/model/prerender_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

SafeBrowsingClientImpl::SafeBrowsingClientImpl(
    PrefService* pref_service,
    safe_browsing::HashRealTimeService* hash_real_time_service,
    UrlLookupServiceFactory url_lookup_service_factory,
    enterprise_connectors::ConnectorsService* connectors_service)
    : pref_service_(pref_service),
      hash_real_time_service_(hash_real_time_service),
      url_lookup_service_factory_(url_lookup_service_factory),
      connectors_service_(connectors_service) {
  CHECK(connectors_service_);
}

SafeBrowsingClientImpl::~SafeBrowsingClientImpl() = default;

base::WeakPtr<SafeBrowsingClient> SafeBrowsingClientImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

PrefService* SafeBrowsingClientImpl::GetPrefs() {
  return pref_service_;
}

SafeBrowsingService* SafeBrowsingClientImpl::GetSafeBrowsingService() {
  return GetApplicationContext()->GetSafeBrowsingService();
}

safe_browsing::RealTimeUrlLookupServiceBase*
SafeBrowsingClientImpl::GetRealTimeUrlLookupService() {
  return url_lookup_service_factory_.Run();
}

safe_browsing::HashRealTimeService*
SafeBrowsingClientImpl::GetHashRealTimeService() {
  return hash_real_time_service_;
}

variations::VariationsService* SafeBrowsingClientImpl::GetVariationsService() {
  return GetApplicationContext()->GetVariationsService();
}

bool SafeBrowsingClientImpl::ShouldBlockUnsafeResource(
    const security_interstitials::UnsafeResource& resource) const {
  // Send do-not-proceed signal if the WebState is for a prerender tab.
  if (web::WebState* web_state = resource.weak_web_state.get()) {
    return PrerenderTabHelper::FromWebState(web_state) != nullptr;
  }
  return false;
}

bool SafeBrowsingClientImpl::OnMainFrameUrlQueryCancellationDecided(
    web::WebState* web_state,
    const GURL& url) {
  // When a prendered page is unsafe, cancel the prerender.
  if (web_state) {
    if (auto* tab_helper = PrerenderTabHelper::FromWebState(web_state)) {
      tab_helper->CancelPrerender();
      return false;
    }
  }

  return true;
}

bool SafeBrowsingClientImpl::ShouldForceSyncRealTimeUrlChecks() const {
  return connectors_service_ &&
         enterprise_connectors::IsEnterpriseUrlFilteringEnabled(
             connectors_service_->GetAppliedRealTimeUrlCheck());
}

void SafeBrowsingClientImpl::OnSecurityInterstitialShown(
    web::WebState* web_state,
    const security_interstitials::UnsafeResource& resource) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  enterprise_connectors::ReportingEventRouter* router =
      enterprise_connectors::IOSReportingEventRouterFactory::GetForProfile(
          profile);
  if (router) {
    google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
        referrer_chain;
    router->OnSecurityInterstitialShown(
        resource.url,
        safe_browsing::GetThreatTypeStringForInterstitial(resource.threat_type),
        /*net_error_code=*/0,
        pref_service_->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled),
        referrer_chain);
  }
}
