// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/subresource_redirect_util.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
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

  // Enable subresource redirect only for <img> elements created by parser.
  // Images created from javascript, fetched via XHR/Fetch API should not be
  // subresource redirected due to the additional CORB/CORS handling needed for
  // them.
  if (!image_element->ElementCreatedByParser()) {
    RecordSubresourceRedirectIneligibility(
        SecurityOrigin::AreSameOrigin(url, image_element->GetDocument().Url())
            ? BlinkSubresourceRedirectIneligibility::
                  kJavascriptCreatedSameOrigin
            : BlinkSubresourceRedirectIneligibility::
                  kJavascriptCreatedCrossOrigin);
    return false;
  }

  // Create a cross origin URL by appending a string to the original host. This
  // is used to find whether CSP is restricting image fetches from other
  // origins.
  KURL cross_origin_url = url;
  cross_origin_url.SetHost(url.Host() + "crossorigin.com");
  auto* content_security_policy =
      image_element->GetExecutionContext()->GetContentSecurityPolicy();
  if (content_security_policy &&
      !content_security_policy->AllowImageFromSource(
          cross_origin_url, cross_origin_url, RedirectStatus::kNoRedirect,
          ReportingDisposition::kSuppressReporting)) {
    // Check if an object is allowed by CSP as a proxy to determine whether the
    // image resource was disallowed by default-src or img-src directives. When
    // an object is allowed it means default-src did not disallow the image.
    RecordSubresourceRedirectIneligibility(
        content_security_policy->AllowRequest(
            mojom::blink::RequestContextType::OBJECT,
            network::mojom::RequestDestination::kObject, cross_origin_url,
            String() /* nonce */, IntegrityMetadataSet(),
            ParserDisposition::kParserInserted, cross_origin_url,
            RedirectStatus::kNoRedirect,
            ReportingDisposition::kSuppressReporting,
            ContentSecurityPolicy::CheckHeaderType::kCheckEnforce)
            ? BlinkSubresourceRedirectIneligibility::
                  kContentSecurityPolicyImgSrcRestricted
            : BlinkSubresourceRedirectIneligibility::
                  kContentSecurityPolicyDefaultSrcRestricted);
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
  return true;
}

}  // namespace blink
