// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cookie_utils.h"

#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/common/content_client.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

namespace {

void RecordContextDowngradeUKM(RenderFrameHost* rfh,
                               CookieAccessDetails::Type access_type,
                               const net::CookieInclusionStatus& status,
                               const GURL& url) {
  DCHECK(rfh);
  ukm::SourceId source_id = rfh->GetPageUkmSourceId();

  if (access_type == CookieAccessDetails::Type::kRead) {
    ukm::builders::SchemefulSameSiteContextDowngrade(source_id)
        .SetRequestPerCookie(status.GetBreakingDowngradeMetricsEnumValue(url))
        .Record(ukm::UkmRecorder::Get());
  } else {
    DCHECK(access_type == CookieAccessDetails::Type::kChange);
    ukm::builders::SchemefulSameSiteContextDowngrade(source_id)
        .SetResponsePerCookie(status.GetBreakingDowngradeMetricsEnumValue(url))
        .Record(ukm::UkmRecorder::Get());
  }
}

}  // namespace

void SplitCookiesIntoAllowedAndBlocked(
    const network::mojom::CookieAccessDetailsPtr& cookie_details,
    CookieAccessDetails* allowed,
    CookieAccessDetails* blocked) {
  *allowed =
      CookieAccessDetails({cookie_details->type,
                           cookie_details->url,
                           cookie_details->site_for_cookies.RepresentativeUrl(),
                           {},
                           /* blocked_by_policy=*/false});
  *blocked =
      CookieAccessDetails({cookie_details->type,
                           cookie_details->url,
                           cookie_details->site_for_cookies.RepresentativeUrl(),
                           {},
                           /* blocked_by_policy=*/true});

  for (auto& cookie_and_access_result : cookie_details->cookie_list) {
    if (cookie_and_access_result->access_result.status.HasOnlyExclusionReason(
            net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES)) {
      blocked->cookie_list.push_back(
          std::move(cookie_and_access_result->cookie_or_line->get_cookie()));
    } else if (cookie_and_access_result->access_result.status.IsInclude()) {
      allowed->cookie_list.push_back(
          std::move(cookie_and_access_result->cookie_or_line->get_cookie()));
    }
  }
}

void EmitCookieWarningsAndMetrics(
    RenderFrameHostImpl* rfh,
    const network::mojom::CookieAccessDetailsPtr& cookie_details) {
  RenderFrameHostImpl* root_frame_host = rfh->GetMainFrame();

  if (!root_frame_host->IsCurrent())
    return;

  bool samesite_treated_as_lax_cookies = false;
  bool samesite_none_insecure_cookies = false;
  bool breaking_context_downgrade = false;
  bool lax_allow_unsafe_cookies = false;

  bool same_party = false;
  bool same_party_exclusion_overruled_samesite = false;
  bool same_party_inclusion_overruled_samesite = false;

  for (const network::mojom::CookieOrLineWithAccessResultPtr& cookie :
       cookie_details->cookie_list) {
    if (cookie->access_result.status.ShouldWarn()) {
      const net::CookieInclusionStatus& status = cookie->access_result.status;
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

      if (cookie->cookie_or_line->is_cookie()) {
        // TODO(sigurds): report issues on cookie line problems as well.
        devtools_instrumentation::ReportSameSiteCookieIssue(
            root_frame_host,
            {cookie->cookie_or_line->get_cookie(), cookie->access_result},
            cookie_details->url, cookie_details->site_for_cookies,
            cookie_details->type == CookieAccessDetails::Type::kRead
                ? blink::mojom::SameSiteCookieOperation::kReadCookie
                : blink::mojom::SameSiteCookieOperation::kSetCookie,
            cookie_details->devtools_request_id);
      }

      same_party = same_party ||
                   status.HasWarningReason(
                       net::CookieInclusionStatus::WARN_TREATED_AS_SAMEPARTY);

      same_party_exclusion_overruled_samesite =
          same_party_exclusion_overruled_samesite ||
          status.HasWarningReason(
              net::CookieInclusionStatus::
                  WARN_SAMEPARTY_EXCLUSION_OVERRULED_SAMESITE);

      same_party_inclusion_overruled_samesite =
          same_party_inclusion_overruled_samesite ||
          status.HasWarningReason(
              net::CookieInclusionStatus::
                  WARN_SAMEPARTY_INCLUSION_OVERRULED_SAMESITE);
    }

    breaking_context_downgrade =
        breaking_context_downgrade ||
        cookie->access_result.status.HasDowngradeWarning();

    if (cookie->access_result.status.HasDowngradeWarning()) {
      // Unlike with UMA, do not record cookies that have no downgrade warning.
      RecordContextDowngradeUKM(rfh, cookie_details->type,
                                cookie->access_result.status,
                                cookie_details->url);
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

  if (same_party) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kSamePartyCookieAttribute);
  }

  if (same_party_exclusion_overruled_samesite) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh,
        blink::mojom::WebFeature::kSamePartyCookieExclusionOverruledSameSite);
  }

  if (same_party_inclusion_overruled_samesite) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh,
        blink::mojom::WebFeature::kSamePartyCookieInclusionOverruledSameSite);
  }
}

}  // namespace content
