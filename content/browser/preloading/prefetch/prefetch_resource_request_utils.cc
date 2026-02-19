// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_resource_request_utils.h"

#include "third_party/blink/public/common/navigation/preloading_headers.h"
#include "url/origin.h"

namespace content {

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
