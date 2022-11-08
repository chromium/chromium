// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/origin_trials/critical_origin_trials_throttle.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/origin_trials/origin_trials_utils.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "url/origin.h"

namespace content {

using blink::mojom::ResourceType;

CriticalOriginTrialsThrottle::CriticalOriginTrialsThrottle(
    OriginTrialsControllerDelegate& origin_trials_delegate)
    : origin_trials_delegate_(origin_trials_delegate) {}

CriticalOriginTrialsThrottle::~CriticalOriginTrialsThrottle() = default;

void CriticalOriginTrialsThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  // Right now, Persistent Origin Trials are only supported on navigation
  // requests, but this throttle is called for all network loads. Until support
  // is implemented for other request types, we need to only intercept
  // navigation requests.
  ResourceType request_resource_type =
      static_cast<ResourceType>(request->resource_type);
  is_navigation_request_ = request_resource_type == ResourceType::kMainFrame ||
                           request_resource_type == ResourceType::kSubFrame;

  if (is_navigation_request_)
    SetPreRequestFields(request->url);
}

void CriticalOriginTrialsThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    bool* defer) {
  if (is_navigation_request_) {
    DCHECK_EQ(response_url, request_url_);
    MaybeRestartWithTrials(response_head);
  }
}

void CriticalOriginTrialsThrottle::BeforeWillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  if (is_navigation_request_) {
    MaybeRestartWithTrials(response_head);
    // Update the stored information for the new request
    SetPreRequestFields(redirect_info->new_url);
  }
}

void CriticalOriginTrialsThrottle::MaybeRestartWithTrials(
    const network::mojom::URLResponseHead& response_head) {
  if (!response_head.headers)
    return;

  std::vector<std::string> critical_trials =
      GetCriticalOriginTrialHeaderValues(response_head.headers.get());

  if (critical_trials.empty())
    return;

  url::Origin request_origin = url::Origin::Create(request_url_);

  if (restarted_origins_.contains(request_origin))
    return;

  // Validate the trials requested and check if they can be persisted.
  blink::TrialTokenValidator validator;
  std::vector<std::string> origin_trial_tokens =
      GetOriginTrialHeaderValues(response_head.headers.get());
  base::flat_set<std::string> valid_requested_trials;
  for (const std::string& token : origin_trial_tokens) {
    blink::TrialTokenResult result = validator.ValidateTokenAndTrial(
        token, request_origin, base::Time::Now());
    if (result.Status() == blink::OriginTrialTokenStatus::kSuccess &&
        blink::origin_trials::IsTrialPersistentToNextResponse(
            result.ParsedToken()->feature_name())) {
      valid_requested_trials.insert(result.ParsedToken()->feature_name());
    }
  }

  // Check if a critical trial was requested but not present on the original
  // request.
  bool needs_restart = false;
  for (const std::string& trial : critical_trials) {
    if (valid_requested_trials.contains(trial) &&
        !original_persisted_trials_.contains(trial)) {
      needs_restart = true;
    }
  }

  // The header was present, emit a histogram to track if we need a restart.
  UMA_HISTOGRAM_BOOLEAN("OriginTrials.PersistentOriginTrial.CriticalRestart",
                        needs_restart);

  if (needs_restart) {
    // Persist the trials that were set, so we can try again.
    origin_trials_delegate_->PersistTrialsFromTokens(
        request_origin, origin_trial_tokens, base::Time::Now());
    restarted_origins_.insert(request_origin);
    delegate_->RestartWithURLResetAndFlags(0);
  }
}

void CriticalOriginTrialsThrottle::SetPreRequestFields(
    const GURL& request_url) {
  request_url_ = request_url;
  original_persisted_trials_ =
      origin_trials_delegate_->GetPersistedTrialsForOrigin(
          url::Origin::Create(request_url_), base::Time::Now());
}

}  // namespace content
