// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_resource_request_utils.h"

#include "content/browser/preloading/preload_pipeline_info_impl.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"
#include "url/origin.h"

namespace content {

void AddAdditionalHeaders(net::HttpRequestHeaders& request_headers,
                          const PrefetchRequest& prefetch_request) {
  const auto& additional_headers = prefetch_request.additional_headers();
  // Ignore "User-Agent" override by `additional_headers` if UA override fix are
  // enabled.
  // TODO(crbug.com/383779480): Add tests.
  if (base::FeatureList::IsEnabled(
          features::kPreloadingRespectUserAgentOverride)) {
    net::HttpRequestHeaders additional_headers_without_ua = additional_headers;
    additional_headers_without_ua.RemoveHeader(
        net::HttpRequestHeaders::kUserAgent);
    request_headers.MergeFrom(additional_headers_without_ua);
  } else {
    request_headers.MergeFrom(additional_headers);
  }
}

// TODO(crbug.com/452392023): Currently this is for speculation rules
// prefetch only, but it should be extended to other prefetch embedder
// triggers.
// TODO(crbug.com/452392023): Add missing fields and update
// `tools/traffic_annotation/safe_list.txt`.
constexpr net::NetworkTrafficAnnotationTag
    kNavigationalPrefetchTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("speculation_rules_prefetch",
                                            R"(
          semantics {
            sender: "Speculation Rules Prefetch Loader"
            description:
              "Prefetches the mainframe HTML of a page specified via "
              "speculation rules. This is done out-of-band of normal "
              "prefetches to allow total isolation of this request from the "
              "rest of browser traffic and user state like cookies and cache."
            trigger:
              "Used only when this feature and speculation rules feature are "
              "enabled."
            data: "None."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control this via a setting specific to each content "
              "embedder."
            policy_exception_justification: "Not implemented."
        })");

void AddSecPurposeHeader(net::HttpRequestHeaders& request_headers,
                         const GURL& request_url,
                         const PrefetchRequest& prefetch_request) {
  const char* header_value = [&]() {
    switch (prefetch_request.preload_pipeline_info()
                .planned_max_preloading_type()) {
      case PreloadingType::kPrefetch:
        if (prefetch_request.IsProxyRequiredForURL(request_url)) {
          return blink::kSecPurposePrefetchAnonymousClientIpHeaderValue;
        } else {
          return blink::kSecPurposePrefetchHeaderValue;
        }
      case PreloadingType::kPrerenderUntilScript:
      case PreloadingType::kPrerender:
        if (prefetch_request.IsProxyRequiredForURL(request_url)) {
          // Note that this path would be reachable if a prefetch ahead of
          // prerender were triggered with a speculation candidate with
          // `requires_anonymous_client_ip_when_cross_origin`. But such
          // Speculation Rules are discarded in blink.
          //
          // See
          // https://github.com/WICG/nav-speculation/blob/main/triggers.md#requirements
          NOTREACHED();
        } else {
          return blink::kSecPurposePrefetchPrerenderHeaderValue;
        }
      case PreloadingType::kUnspecified:
      case PreloadingType::kPreconnect:
      case PreloadingType::kNoStatePrefetch:
      case PreloadingType::kLinkPreview:
        NOTREACHED();
    }
  }();

  request_headers.SetHeader(blink::kSecPurposeHeaderName, header_value);
}

void AddSpeculationTagsHeader(net::HttpRequestHeaders& request_headers,
                              const GURL& request_url,
                              const PrefetchRequest& prefetch_request) {
  // Sec-Speculation-Tags is set only when the prefetch is triggered
  // by speculation rules and it is not cross-site prefetch.
  // To see more details:
  // https://github.com/WICG/nav-speculation/blob/main/speculation-rules-tags.md#the-cross-site-case
  if (prefetch_request.speculation_rules_tags().has_value() &&
      !prefetch_request.IsCrossSiteRequest(url::Origin::Create(request_url))) {
    std::optional<std::string> serialized_list =
        prefetch_request.speculation_rules_tags()
            ->ConvertStringToHeaderString();
    CHECK(serialized_list.has_value());
    request_headers.SetHeader(blink::kSecSpeculationTagsHeaderName,
                              serialized_list.value());
  }
}

}  // namespace content
