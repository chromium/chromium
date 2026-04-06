// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/connection_allowlist_gating.h"

#include <optional>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/gurl.h"

namespace content {

bool ResponseContainsConnectionAllowlist(
    const network::mojom::URLResponseHead* response_head) {
  return response_head && response_head->headers &&
         response_head->parsed_headers &&
         (response_head->parsed_headers->connection_allowlists.enforced
              .has_value() ||
          response_head->parsed_headers->connection_allowlists.report_only
              .has_value());
}

bool ResponseEnablesConnectionAllowlistsOriginTrial(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers) {
  return base::FeatureList::IsEnabled(
             blink::features::kOverrideConnectionAllowlistOriginTrial) ||
         blink::TrialTokenValidator().RequestEnablesFeature(
             request_url, response_headers, "ConnectionAllowlist",
             base::Time::Now());
}
}  // namespace content
