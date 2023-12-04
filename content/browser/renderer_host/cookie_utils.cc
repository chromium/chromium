// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cookie_utils.h"

#include <cstddef>
#include <ostream>
#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/legacy_tech_cookie_issue_details.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

namespace content {

namespace {

void PotentiallyRecordNonAsciiCookieNameValue(
    RenderFrameHost* rfh,
    CookieAccessDetails::Type access_type,
    const std::string& name,
    const std::string& value) {
  CHECK(rfh);

  if (access_type != CookieAccessDetails::Type::kChange) {
    return;
  }

  // Our data collection policy disallows collecting UKMs while prerendering.
  // See //content/browser/preloading/prerender/README.md and ask the team to
  // explore options to record data for prerendering pages if we need to
  // support the case.
  if (rfh->IsInLifecycleState(RenderFrameHost::LifecycleState::kPrerendering)) {
    return;
  }

  bool name_has_non_ascii = !base::IsStringASCII(name);
  bool value_has_non_ascii = !base::IsStringASCII(value);

  if (name_has_non_ascii || value_has_non_ascii) {
    ukm::SourceId source_id = rfh->GetPageUkmSourceId();

    auto event = ukm::builders::CookieHasNonAsciiCharacter(source_id);

    // The event itself is what we're interested in, the value of "true" here
    // can be ignored.
    if (name_has_non_ascii) {
      event.SetName(true);
    }

    if (value_has_non_ascii) {
      event.SetValue(true);
    }

    event.Record(ukm::UkmRecorder::Get());
  }
}

void RecordFirstPartyPartitionedCookieCrossSiteContextUKM(
    RenderFrameHostImpl* render_frame_host_impl,
    const net::CanonicalCookie& cookie) {
  // Our data collection policy disallows collecting UKMs while prerendering.
  // See //content/browser/preloading/prerender/README.md and ask the team to
  // explore options to record data for prerendering pages if we need to
  // support the case.
  if (render_frame_host_impl->IsInLifecycleState(
          RenderFrameHost::LifecycleState::kPrerendering)) {
    return;
  }

  if (!cookie.IsFirstPartyPartitioned()) {
    return;
  }

  // Same-site embed with cross-site ancestors (ABA embeds) have a null site
  // for cookies since it is a cross-site context. If the result of
  // ComputeSiteForCookies is first-party that means we are not in an ABA
  // embedded context.
  if (render_frame_host_impl->ComputeSiteForCookies().IsFirstParty(
          GURL(base::StrCat({url::kHttpsScheme, url::kStandardSchemeSeparator,
                             cookie.DomainWithoutDot()})))) {
    return;
  }

  ukm::builders::Cookies_FirstPartyPartitionedInCrossSiteContext(
      render_frame_host_impl->GetPageUkmSourceId())
      .SetCookiePresent(true)
      .Record(ukm::UkmRecorder::Get());
}

void RecordPartitionedCookieUseUKM(RenderFrameHost* rfh,
                                   bool partitioned_cookies_exist) {
  // Our data collection policy disallows collecting UKMs while prerendering.
  // See //content/browser/preloading/prerender/README.md and ask the team to
  // explore options to record data for prerendering pages if we need to
  // support the case.
  if (rfh->IsInLifecycleState(RenderFrameHost::LifecycleState::kPrerendering)) {
    return;
  }
  if (!partitioned_cookies_exist) {
    return;
  }
  ukm::SourceId source_id = rfh->GetPageUkmSourceId();

  ukm::builders::PartitionedCookiePresent(source_id)
      .SetPartitionedCookiePresent(partitioned_cookies_exist)
      .Record(ukm::UkmRecorder::Get());
}

void RecordRedirectContextDowngradeUKM(RenderFrameHost* rfh,
                                       CookieAccessDetails::Type access_type,
                                       const net::CanonicalCookie& cookie,
                                       const GURL& url) {
  CHECK(rfh);

  // Our data collection policy disallows collecting UKMs while prerendering.
  // See //content/browser/preloading/prerender/README.md and ask the team to
  // explore options to record data for prerendering pages if we need to
  // support the case.
  if (rfh->IsInLifecycleState(RenderFrameHost::LifecycleState::kPrerendering)) {
    return;
  }

  ukm::SourceId source_id = rfh->GetPageUkmSourceId();

  int64_t samesite_value = static_cast<int64_t>(cookie.SameSite());
  if (access_type == CookieAccessDetails::Type::kRead) {
    base::TimeDelta cookie_age = base::Time::Now() - cookie.CreationDate();

    ukm::builders::SamesiteRedirectContextDowngrade(source_id)
        .SetSamesiteValueReadPerCookie(samesite_value)
        .SetAgePerCookie(
            ukm::GetExponentialBucketMinForUserTiming(cookie_age.InMinutes()))
        .Record(ukm::UkmRecorder::Get());
  } else {
    CHECK(access_type == CookieAccessDetails::Type::kChange);
    ukm::builders::SamesiteRedirectContextDowngrade(source_id)
        .SetSamesiteValueWritePerCookie(samesite_value)
        .Record(ukm::UkmRecorder::Get());
  }
}

void RecordSchemefulContextDowngradeUKM(
    RenderFrameHost* rfh,
    CookieAccessDetails::Type access_type,
    const net::CookieInclusionStatus& status,
    const GURL& url) {
  CHECK(rfh);

  // Our data collection policy disallows collecting UKMs while prerendering.
  // See //content/browser/preloading/prerender/README.md and ask the team to
  // explore options to record data for prerendering pages if we need to
  // support the case.
  if (rfh->IsInLifecycleState(RenderFrameHost::LifecycleState::kPrerendering)) {
    return;
  }

  ukm::SourceId source_id = rfh->GetPageUkmSourceId();

  auto downgrade_metric =
      static_cast<int64_t>(status.GetBreakingDowngradeMetricsEnumValue(url));
  if (access_type == CookieAccessDetails::Type::kRead) {
    ukm::builders::SchemefulSameSiteContextDowngrade(source_id)
        .SetRequestPerCookie(downgrade_metric)
        .Record(ukm::UkmRecorder::Get());
  } else {
    CHECK(access_type == CookieAccessDetails::Type::kChange);
    ukm::builders::SchemefulSameSiteContextDowngrade(source_id)
        .SetResponsePerCookie(downgrade_metric)
        .Record(ukm::UkmRecorder::Get());
  }
}

// LINT.IfChange(should_report_dev_tools)
bool ShouldReportDevToolsIssueForStatus(
    const net::CookieInclusionStatus& status) {
  return status.ShouldWarn() ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::EXCLUDE_DOMAIN_NON_ASCII) ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::
                 EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET) ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);
}
// LINT.ThenChange(//content/browser/renderer_host/cookie_utils.cc:should_report_legacy_tech_report)

// LINT.IfChange(should_report_legacy_tech_report)
bool ShouldReportLegacyTechIssueForStatus(
    const net::CookieInclusionStatus& status) {
  return status.HasExclusionReason(
             net::CookieInclusionStatus::
                 EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET) ||
         status.HasExclusionReason(
             net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT) ||
         status.HasWarningReason(
             net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT);
}
// LINT.ThenChange(//content/browser/renderer_host/cookie_utils.cc:should_report_dev_tools)

// Logs cookie issues to DevTools Issues Panel and logs events to UseCounters
// and UKM for a single cookie-accessed event.
// TODO(crbug.com/977040): Remove when no longer needed.
void EmitCookieWarningsAndMetricsOnce(
    RenderFrameHostImpl* rfh,
    const network::mojom::CookieAccessDetailsPtr& cookie_details) {
  RenderFrameHostImpl* root_frame_host = rfh->GetMainFrame();

  if (!root_frame_host->IsActive())
    return;

  bool samesite_treated_as_lax_cookies = false;
  bool samesite_none_insecure_cookies = false;
  bool breaking_context_downgrade = false;
  bool lax_allow_unsafe_cookies = false;

  bool samesite_cookie_inclusion_changed_by_cross_site_redirect = false;

  bool partitioned_cookies_exist = false;

  bool cookie_has_not_been_refreshed_in_201_to_300_days = false;
  bool cookie_has_not_been_refreshed_in_301_to_350_days = false;
  bool cookie_has_not_been_refreshed_in_351_to_400_days = false;

  bool cookie_has_domain_non_ascii = false;

  for (const network::mojom::CookieOrLineWithAccessResultPtr& cookie :
       cookie_details->cookie_list) {
    const net::CookieInclusionStatus& status = cookie->access_result.status;
    if (ShouldReportDevToolsIssueForStatus(status)) {
      devtools_instrumentation::ReportCookieIssue(
          root_frame_host, cookie, cookie_details->url,
          cookie_details->site_for_cookies,
          cookie_details->type == CookieAccessDetails::Type::kRead
              ? blink::mojom::CookieOperation::kReadCookie
              : blink::mojom::CookieOperation::kSetCookie,
          cookie_details->devtools_request_id);
    }

    // Log to the JS console if there is cookie affected by 3PCD.
    if (status.HasExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT)) {
      root_frame_host->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Blocked third-party cookie. Learn more in the Issues tab.");
    } else if (status.HasWarningReason(
                   net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT)) {
      root_frame_host->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Third-party cookie will be blocked. Learn more in the Issues tab.");
    }

    if (cookie->access_result.status.ShouldWarn()) {
      samesite_treated_as_lax_cookies =
          samesite_treated_as_lax_cookies ||
          status.HasWarningReason(
              net::CookieInclusionStatus::
                  WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT) ||
          status.HasWarningReason(
              net::CookieInclusionStatus::
                  WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE);

      samesite_none_insecure_cookies =
          samesite_none_insecure_cookies ||
          status.HasWarningReason(
              net::CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE);

      lax_allow_unsafe_cookies =
          lax_allow_unsafe_cookies ||
          status.HasWarningReason(
              net::CookieInclusionStatus::
                  WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE);

      samesite_cookie_inclusion_changed_by_cross_site_redirect =
          samesite_cookie_inclusion_changed_by_cross_site_redirect ||
          status.HasWarningReason(
              net::CookieInclusionStatus::
                  WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION);
    }

    cookie_has_domain_non_ascii =
        cookie_has_domain_non_ascii ||
        status.HasWarningReason(
            net::CookieInclusionStatus::WARN_DOMAIN_NON_ASCII) ||
        status.HasExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_DOMAIN_NON_ASCII);

    partitioned_cookies_exist =
        partitioned_cookies_exist ||
        (cookie->cookie_or_line->is_cookie() &&
         cookie->cookie_or_line->get_cookie().IsPartitioned() &&
         // Ignore nonced partition keys since this metric is meant to track
         // usage of the Partitioned attribute.
         !cookie->cookie_or_line->get_cookie().PartitionKey()->nonce());

    RecordPartitionedCookieUseUKM(rfh, partitioned_cookies_exist);

    if (partitioned_cookies_exist) {
      RecordFirstPartyPartitionedCookieCrossSiteContextUKM(
          rfh, cookie->cookie_or_line->get_cookie());
    }

    breaking_context_downgrade =
        breaking_context_downgrade ||
        cookie->access_result.status.HasSchemefulDowngradeWarning();

    if (cookie->access_result.status.HasSchemefulDowngradeWarning()) {
      // Unlike with UMA, do not record cookies that have no schemeful downgrade
      // warning.
      RecordSchemefulContextDowngradeUKM(rfh, cookie_details->type,
                                         cookie->access_result.status,
                                         cookie_details->url);
    }

    if (status.HasWarningReason(
            net::CookieInclusionStatus::
                WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION) &&
        cookie->cookie_or_line->is_cookie()) {
      RecordRedirectContextDowngradeUKM(rfh, cookie_details->type,
                                        cookie->cookie_or_line->get_cookie(),
                                        cookie_details->url);
    }

    if (cookie->cookie_or_line->is_cookie()) {
      PotentiallyRecordNonAsciiCookieNameValue(
          rfh, cookie_details->type,
          cookie->cookie_or_line->get_cookie().Name(),
          cookie->cookie_or_line->get_cookie().Value());
    }

    // In order to anticipate the potential effects of the expiry limit in
    // rfc6265bis, we need to check how long it's been since the cookie was
    // refreshed (if LastUpdateDate is populated). These three buckets were
    // picked so we could engage sites with some granularity around urgency.
    // We ignore the space under 200 days as these cookies are not at risk
    // of expiring and we ignore the space over 400 days as these cookies
    // have already expired. Metrics will take 200 days from M103 to populate.
    base::Time last_update_date =
        cookie->cookie_or_line->is_cookie()
            ? cookie->cookie_or_line->get_cookie().LastUpdateDate()
            : base::Time();
    if (!last_update_date.is_null()) {
      int days_since_refresh = (base::Time::Now() - last_update_date).InDays();
      cookie_has_not_been_refreshed_in_201_to_300_days |=
          days_since_refresh > 200 && days_since_refresh <= 300;
      cookie_has_not_been_refreshed_in_301_to_350_days |=
          days_since_refresh > 300 && days_since_refresh <= 350;
      cookie_has_not_been_refreshed_in_351_to_400_days |=
          days_since_refresh > 350 && days_since_refresh <= 400;
    }
  }

  if (samesite_treated_as_lax_cookies) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCookieNoSameSite);
  }

  if (samesite_none_insecure_cookies) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCookieInsecureAndSameSiteNone);
  }

  if (breaking_context_downgrade) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kSchemefulSameSiteContextDowngrade);
  }

  if (lax_allow_unsafe_cookies) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kLaxAllowingUnsafeCookies);
  }

  if (samesite_cookie_inclusion_changed_by_cross_site_redirect) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::
                 kSameSiteCookieInclusionChangedByCrossSiteRedirect);
  }

  if (partitioned_cookies_exist) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kPartitionedCookies);
  }

  if (cookie_has_not_been_refreshed_in_201_to_300_days) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh,
        blink::mojom::WebFeature::kCookieHasNotBeenRefreshedIn201To300Days);
  }

  if (cookie_has_not_been_refreshed_in_301_to_350_days) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh,
        blink::mojom::WebFeature::kCookieHasNotBeenRefreshedIn301To350Days);
  }

  if (cookie_has_not_been_refreshed_in_351_to_400_days) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh,
        blink::mojom::WebFeature::kCookieHasNotBeenRefreshedIn351To400Days);
  }

  if (cookie_has_domain_non_ascii) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCookieDomainNonASCII);
  }
}

// Logs cookie issues to Legacy Technology Report.
void ReportLegacyTechEvent(
    RenderFrameHostImpl* render_frame_host,
    NavigationRequest* navigation_request,
    const network::mojom::CookieAccessDetailsPtr& cookie_details) {
  if (!base::FeatureList::IsEnabled(
          features::kLegacyTechReportEnableCookieIssueReports)) {
    return;
  }
  CHECK(render_frame_host);

  for (const network::mojom::CookieOrLineWithAccessResultPtr& cookie :
       cookie_details->cookie_list) {
    const net::CookieInclusionStatus& status = cookie->access_result.status;
    if (ShouldReportLegacyTechIssueForStatus(status) &&
        cookie->cookie_or_line->is_cookie()) {
      std::string type;
      if (status.HasExclusionReason(
              net::CookieInclusionStatus::
                  EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET) ||
          status.HasExclusionReason(
              net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT)) {
        type = "ThirdPartyCookieAccessError";
      } else if (status.HasWarningReason(
                     net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT)) {
        type = "ThirdPartyCookieAccessWarning";
      } else {
        DLOG(ERROR) << "Unexpected call of ReportLegacyTechEvent.";
      }

      GURL url = render_frame_host->GetOutermostMainFrameOrEmbedder()
                     ->GetLastCommittedURL();
      GURL frame_url = render_frame_host->GetLastCommittedURL();
      if (navigation_request != nullptr) {
        if (!navigation_request->frame_tree_node()
                 ->GetParentOrOuterDocumentOrEmbedder()) {
          url = navigation_request->GetURL();
          frame_url = navigation_request->GetURL();
        } else {
          frame_url = navigation_request->GetURL();
        }
      }

      LegacyTechCookieIssueDetails cookie_issue_details = {
          cookie_details->url.spec(),
          cookie->cookie_or_line->get_cookie().Name(),
          cookie->cookie_or_line->get_cookie().Domain(),
          cookie->cookie_or_line->get_cookie().Path(),
          cookie_details->type == CookieAccessDetails::Type::kChange
              ? LegacyTechCookieIssueDetails::AccessOperation::kWrite
              : LegacyTechCookieIssueDetails::AccessOperation::kRead};

      GetContentClient()->browser()->ReportLegacyTechEvent(
          render_frame_host, type, url, frame_url, /*filename=*/"", /*line=*/0,
          /*column=*/0, cookie_issue_details);
    }
  }
}

}  // namespace

void SplitCookiesIntoAllowedAndBlocked(
    const network::mojom::CookieAccessDetailsPtr& cookie_details,
    CookieAccessDetails* allowed,
    CookieAccessDetails* blocked) {
  // For some cases `site_for_cookies` representative url is empty when
  // OnCookieAccess is triggered for a third party. For example iframe third
  // party accesses cookies when TPCD Metadata allows third party cookie access.
  //
  // Make `first_party_url` considering both `top_frame_origin` and
  // `site_for_cookies` which is similar with GetFirstPartyURL() in
  // components/content_settings/core/common/cookie_settings_base.h.
  // If the `top_frame_origin` is non-opaque, it is chosen; otherwise, the
  // `site_for_cookies` representative url is used.
  const GURL first_party_url =
      cookie_details->top_frame_origin.opaque()
          ? cookie_details->site_for_cookies.RepresentativeUrl()
          : cookie_details->top_frame_origin.GetURL();

  *allowed = CookieAccessDetails({cookie_details->type,
                                  cookie_details->url,
                                  first_party_url,
                                  {},
                                  cookie_details->count,
                                  /* blocked_by_policy=*/false,
                                  cookie_details->is_ad_tagged,
                                  cookie_details->cookie_setting_overrides});
  int allowed_count = base::ranges::count_if(
      cookie_details->cookie_list,
      [](const network::mojom::CookieOrLineWithAccessResultPtr&
             cookie_and_access_result) {
        // "Included" cookies have no exclusion reasons so we don't also have to
        // check for !(net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES).
        return cookie_and_access_result->access_result.status.IsInclude();
      });
  allowed->cookie_list.reserve(allowed_count);

  *blocked = CookieAccessDetails({cookie_details->type,
                                  cookie_details->url,
                                  first_party_url,
                                  {},
                                  cookie_details->count,
                                  /* blocked_by_policy=*/true,
                                  cookie_details->is_ad_tagged,
                                  cookie_details->cookie_setting_overrides});
  int blocked_count = base::ranges::count_if(
      cookie_details->cookie_list,
      [](const network::mojom::CookieOrLineWithAccessResultPtr&
             cookie_and_access_result) {
        return cookie_and_access_result->access_result.status
            .ExcludedByUserPreferences();
      });
  blocked->cookie_list.reserve(blocked_count);

  for (const auto& cookie_and_access_result : cookie_details->cookie_list) {
    if (cookie_and_access_result->access_result.status
            .ExcludedByUserPreferences()) {
      blocked->cookie_list.emplace_back(
          std::move(cookie_and_access_result->cookie_or_line->get_cookie()));
    } else if (cookie_and_access_result->access_result.status.IsInclude()) {
      allowed->cookie_list.emplace_back(
          std::move(cookie_and_access_result->cookie_or_line->get_cookie()));
    }
  }
}

void EmitCookieWarningsAndMetrics(
    RenderFrameHostImpl* rfh,
    NavigationRequest* navigation_request,
    const network::mojom::CookieAccessDetailsPtr& cookie_details) {
  ReportLegacyTechEvent(rfh, navigation_request, cookie_details);
  for (size_t i = 0; i < cookie_details->count; ++i) {
    EmitCookieWarningsAndMetricsOnce(rfh, cookie_details);
  }
}

}  // namespace content
