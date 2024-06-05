// Copyright 2022 The Chromium Authors
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

// static
bool CriticalOriginTrialsThrottle::IsNavigationRequest(
    const network::ResourceRequest& request) {
  ResourceType request_resource_type =
      static_cast<ResourceType>(request.resource_type);
  return request_resource_type == ResourceType::kMainFrame ||
         request_resource_type == ResourceType::kSubFrame;
}

CriticalOriginTrialsThrottle::CriticalOriginTrialsThrottle(
    OriginTrialsControllerDelegate& origin_trials_delegate,
    std::optional<url::Origin> top_frame_origin,
    std::optional<ukm::SourceId> source_id)
    : origin_trials_delegate_(origin_trials_delegate),
      top_frame_origin_(std::move(top_frame_origin)),
      source_id_(source_id) {}

CriticalOriginTrialsThrottle::~CriticalOriginTrialsThrottle() = default;

void CriticalOriginTrialsThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  // Right now, Persistent Origin Trials are only supported on navigation
  // requests. Until support is implemented for other request types, we need to
  // only intercept navigation requests.
  is_navigation_request_ = IsNavigationRequest(*request);

  if (is_navigation_request_)
    SetPreRequestFields(request->url);
}

void CriticalOriginTrialsThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head,
    RestartWithURLReset* restart_with_url_reset) {
  if (is_navigation_request_) {
    DCHECK_EQ(response_url, request_url_);
    MaybeRestartWithTrials(response_head, restart_with_url_reset);
  }
}

void CriticalOriginTrialsThrottle::BeforeWillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    RestartWithURLReset* restart_with_url_reset,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  if (is_navigation_request_) {
    MaybeRestartWithTrials(response_head, restart_with_url_reset);
    // Update the stored information for the new request
    SetPreRequestFields(redirect_info->new_url);
  }
}

void CriticalOriginTrialsThrottle::MaybeRestartWithTrials(
    const network::mojom::URLResponseHead& response_head,
    RestartWithURLReset* restart_with_url_reset) {
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
    url::Origin partition_origin = top_frame_origin_.value_or(request_origin);
    // Add the new tokens to the set of persisted trials
    origin_trials_delegate_->PersistAdditionalTrialsFromTokens(
        request_origin, partition_origin, /*script_origins=*/{},
        origin_trial_tokens, base::Time::Now(), source_id_);
    restarted_origins_.insert(request_origin);
    *restart_with_url_reset = RestartWithURLReset(true);
  }
}

void CriticalOriginTrialsThrottle::SetPreRequestFields(
    const GURL& request_url) {
  request_url_ = request_url;
  url::Origin partition_origin =
      top_frame_origin_.value_or(url::Origin::Create(request_url_));
  original_persisted_trials_ =
      origin_trials_delegate_->GetPersistedTrialsForOrigin(
          url::Origin::Create(request_url_), partition_origin,
          base::Time::Now());
}

}  // namespace content
