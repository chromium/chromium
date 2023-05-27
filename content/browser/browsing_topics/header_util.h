// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_TOPICS_HEADER_UTIL_H_
#define CONTENT_BROWSER_BROWSING_TOPICS_HEADER_UTIL_H_

#include "components/browsing_topics/common/common_types.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"

namespace content {

extern const char kBrowsingTopicsRequestHeaderKey[];

// Derive the header value for `Sec-Browsing-Topics` given `topics` and
// `num_versions_in_epochs`.
std::string CONTENT_EXPORT
DeriveTopicsHeaderValue(const std::vector<blink::mojom::EpochTopicPtr>& topics,
                        int num_versions_in_epochs);

// Handle the response for topics eligible requests.
void CONTENT_EXPORT HandleTopicsEligibleResponse(
    const network::mojom::ParsedHeadersPtr& parsed_headers,
    const url::Origin& caller_origin,
    RenderFrameHost& request_initiator_frame,
    browsing_topics::ApiCallerSource caller_source);

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_TOPICS_HEADER_UTIL_H_
