// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/subresource_redirect_util.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

// Records the ineligibility metrics.
void RecordSubresourceRedirectIneligibility(
    BlinkSubresourceRedirectIneligibility reason) {
  base::UmaHistogramEnumeration("SubresourceRedirect.Blink.Ineligibility",
                                reason);
}

// Returns whether the URL scheme is http or https.
bool IsSchemeHttpOrHttps(const KURL& url) {
  return url.Protocol() == url::kHttpsScheme ||
         url.Protocol() == url::kHttpScheme;
}

// Returns whether CSP restricted subresource redirect images should be allowed
// for subresource redirect compression.
bool ShouldAllowCspRestrictedImages() {
  return base::GetFieldTrialParamByFeatureAsBool(
      blink::features::kSubresourceRedirect, "allow_csp_restricted_images",
      false);
}

}  // namespace

bool ShouldEnableSubresourceRedirect(HTMLImageElement* image_element,
                                     const KURL& url) {
  if (!image_element)
    return false;

  // Allow redirection only when DataSaver is enabled and subresource redirect
  // feature is enabled which allows redirecting to better optimized versions.
  if (!base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect) ||
      !GetNetworkStateNotifier().SaveDataEnabled()) {
    return false;
  }

  // Allow redirection only for http(s) subresources on http(s) documents.
  if (!IsSchemeHttpOrHttps(url) ||
      !IsSchemeHttpOrHttps(image_element->GetDocument().Url())) {
    return false;
  }

  // Allow subresource redirect only when cross-origin attribute is not set,
  // which indicates CORS validation is not triggered for the image.
  if (GetCrossOriginAttributeValue(image_element->FastGetAttribute(
          html_names::kCrossoriginAttr)) != kCrossOriginAttributeNotSet) {
    RecordSubresourceRedirectIneligibility(
        BlinkSubresourceRedirectIneligibility::kCrossOriginAttributeSet);
    return false;
  }

  // Enable subresource redirect for <img> elements created by parser, or for
  // crossorigin <img> elements without cross-origin attribute when not created
  // by parser. Images created from javascript, fetched via XHR/Fetch API should
  // not be subresource redirected when they are sameorigin, since the redirect
  // brings in cross-origin issues. Such images having cross-origin attribute
  // should not be subresource redirected too due to the additional CORB/CORS
  // handling needed for them.
  if (!image_element->ElementCreatedByParser()) {
    bool is_sameorigin =
        SecurityOrigin::AreSameOrigin(url, image_element->GetDocument().Url());
    bool allow_javascript_crossorigin_images =
        base::GetFieldTrialParamByFeatureAsBool(
            blink::features::kSubresourceRedirect,
            "allow_javascript_crossorigin_images", false);
    if (!allow_javascript_crossorigin_images) {
      RecordSubresourceRedirectIneligibility(
          is_sameorigin ? BlinkSubresourceRedirectIneligibility::
                              kJavascriptCreatedSameOrigin
                        : BlinkSubresourceRedirectIneligibility::
                              kJavascriptCreatedCrossOrigin);
      return false;
    }
    if (is_sameorigin) {
      RecordSubresourceRedirectIneligibility(
          BlinkSubresourceRedirectIneligibility::kJavascriptCreatedSameOrigin);
      return false;
    }
  }

  if (ShouldAllowCspRestrictedImages())
    return true;

  // Check the actual subresource redirect URL constructed from the subresource
  // redirect origin is restricted by CSP.
  auto litepage_subresource_redirect_origin = SecurityOrigin::CreateFromString(
      image_element->GetDocument()
          .GetSettings()
          ->GetLitePageSubresourceRedirectOrigin());
  KURL subresource_redirect_url = url;
  subresource_redirect_url.SetProtocol(
      litepage_subresource_redirect_origin->Protocol());
  subresource_redirect_url.SetHost(
      litepage_subresource_redirect_origin->Host());
  subresource_redirect_url.SetPort(
      litepage_subresource_redirect_origin->Port());
  auto* content_security_policy =
      image_element->GetExecutionContext()->GetContentSecurityPolicy();
  if (content_security_policy &&
      !content_security_policy->AllowImageFromSource(
          subresource_redirect_url, subresource_redirect_url,
          RedirectStatus::kNoRedirect,
          ReportingDisposition::kSuppressReporting)) {
    // When any of the CSP policy has img-src directive, then treat it as the
    // image was disallowed by img-src directive.
    bool disabled_by_img_src = false;
    for (const auto& policy : content_security_policy->GetParsedPolicies()) {
      if (CSPDirectiveListOperativeDirective(*policy, CSPDirectiveName::ImgSrc)
              .type == CSPDirectiveName::ImgSrc) {
        disabled_by_img_src = true;
      }
    }
    RecordSubresourceRedirectIneligibility(
        disabled_by_img_src ? BlinkSubresourceRedirectIneligibility::
                                  kContentSecurityPolicyImgSrcRestricted
                            : BlinkSubresourceRedirectIneligibility::
                                  kContentSecurityPolicyDefaultSrcRestricted);
    return false;
  }
  return true;
}

bool ShouldDisableCSPCheckForLitePageSubresourceRedirectOrigin(
    scoped_refptr<SecurityOrigin> litepage_subresource_redirect_origin,
    mojom::blink::RequestContextType request_context,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url) {
  if (!base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect))
    return false;
  if (request_context != mojom::blink::RequestContextType::IMAGE &&
      request_context != mojom::blink::RequestContextType::IMAGE_SET) {
    return false;
  }
  if (redirect_status != ResourceRequest::RedirectStatus::kFollowedRedirect)
    return false;
  if (!ShouldAllowCspRestrictedImages())
    return false;

  if (litepage_subresource_redirect_origin->IsOpaque())
    return false;

  return litepage_subresource_redirect_origin->IsSameOriginWith(
      SecurityOrigin::Create(url).get());
}

}  // namespace blink
