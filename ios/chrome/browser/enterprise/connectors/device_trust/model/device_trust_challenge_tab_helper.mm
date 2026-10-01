// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_challenge_tab_helper.h"

#import <utility>

#import "base/check.h"
#import "base/task/sequenced_task_runner.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "components/enterprise/device_trust/core/metrics_utils.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {
// Browser-side timeout for device attestation requests.
constexpr base::TimeDelta kAttestationTimeout = base::Seconds(25);

// Determines the URL to evaluate against Device Trust allowlist policies.
// By default, uses the security origin's base URL. If a candidate URL is
// present and is same-origin with `security_origin`, prefers the candidate URL
// so that path-scoped allowlist patterns (e.g. "https://example.com/login") can
// match.
GURL DeterminePolicyCheckUrl(const url::Origin& security_origin,
                             const std::optional<GURL>& candidate_url) {
  if (candidate_url.has_value() &&
      security_origin.IsSameOriginWith(*candidate_url)) {
    return *candidate_url;
  }
  return security_origin.GetURL();
}
}  // namespace

DeviceTrustChallengeTabHelper::PendingRequest::PendingRequest(
    AttestationCallback callback)
    : callback(std::move(callback)) {}

DeviceTrustChallengeTabHelper::PendingRequest::~PendingRequest() = default;

DeviceTrustChallengeTabHelper::DeviceTrustChallengeTabHelper(
    web::WebState* web_state,
    enterprise_connectors::DeviceTrustService* device_trust_service)
    : device_trust_service_(device_trust_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(web_state);

  web_state_observation_.Observe(web_state);

  DeviceTrustJavaScriptFeature* feature =
      DeviceTrustJavaScriptFeature::GetInstance();
  web::WebFramesManager* web_frames_manager =
      feature->GetWebFramesManager(web_state);
  CHECK(web_frames_manager);

  web_frames_manager_observation_.Observe(web_frames_manager);
  MaybeSetupDeviceTrustAPI(web_frames_manager->GetMainWebFrame());
}

void DeviceTrustChallengeTabHelper::WebFrameBecameAvailable(
    web::WebFramesManager*,
    web::WebFrame* web_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeSetupDeviceTrustAPI(web_frame);
}

DeviceTrustChallengeTabHelper::~DeviceTrustChallengeTabHelper() = default;

void DeviceTrustChallengeTabHelper::BuildChallengeResponse(
    const url::Origin& security_origin,
    const std::optional<GURL>& request_url,
    const std::string& challenge,
    AttestationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_trust_service_ || !device_trust_service_->IsEnabled()) {
    PostError(std::move(callback),
              enterprise_connectors::DeviceTrustError::kServiceUnavailable);
    return;
  }

  // Opaque origins have no authority to request device attestation.
  if (security_origin.opaque()) {
    PostError(std::move(callback),
              enterprise_connectors::DeviceTrustError::kInvalidOrigin);
    return;
  }

  const GURL policy_check_url =
      DeterminePolicyCheckUrl(security_origin, request_url);

  const std::set<enterprise_connectors::DTCPolicyLevel> levels =
      device_trust_service_->Watches(policy_check_url);
  if (levels.empty()) {
    PostError(std::move(callback),
              enterprise_connectors::DeviceTrustError::kUrlNotAllowed);
    return;
  }

  if (pending_requests_.size() >= kMaxPendingRequests) {
    PostError(std::move(callback),
              enterprise_connectors::DeviceTrustError::kTooManyRequests);
    return;
  }

  // Like desktop, only count challenges that passed all local checks, so that
  // kChallengeReceived and PolicyLevel match Handshake.Result.
  enterprise_connectors::LogAttestationFunnelStep(
      enterprise_connectors::DTAttestationFunnelStep::kChallengeReceived);
  enterprise_connectors::LogAttestationPolicyLevel(levels);

  const RequestId request_id{next_request_id_++};
  auto request = std::make_unique<PendingRequest>(std::move(callback));
  request->timer.Start(
      FROM_HERE, kAttestationTimeout,
      base::BindOnce(&DeviceTrustChallengeTabHelper::OnAttestationTimeout,
                     weak_factory_.GetWeakPtr(), request_id));
  pending_requests_.emplace(request_id, std::move(request));

  device_trust_service_->BuildChallengeResponse(
      challenge, levels,
      base::BindOnce(&DeviceTrustChallengeTabHelper::OnChallengeResponseReady,
                     weak_factory_.GetWeakPtr(), request_id));
}

void DeviceTrustChallengeTabHelper::PostError(
    AttestationCallback callback,
    enterprise_connectors::DeviceTrustError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeviceTrustChallengeTabHelper::RunPostedError,
                     weak_factory_.GetWeakPtr(), std::move(callback), error));
}

void DeviceTrustChallengeTabHelper::RunPostedError(
    AttestationCallback callback,
    enterprise_connectors::DeviceTrustError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Early rejections happen before the handshake starts, so they are not
  // handshake outcomes: LogDeviceTrustResponse() must not be called for them.
  enterprise_connectors::DeviceTrustResponse response;
  response.error = error;
  std::move(callback).Run(response);
}

std::unique_ptr<DeviceTrustChallengeTabHelper::PendingRequest>
DeviceTrustChallengeTabHelper::TakePendingRequest(RequestId request_id) {
  auto it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    return nullptr;
  }
  std::unique_ptr<PendingRequest> request = std::move(it->second);
  pending_requests_.erase(it);
  return request;
}

void DeviceTrustChallengeTabHelper::OnChallengeResponseReady(
    RequestId request_id,
    const enterprise_connectors::DeviceTrustResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto pending_request = TakePendingRequest(request_id);
  if (!pending_request) {
    // Request was already taken (e.g. timed out or dropped on navigation).
    // Ignore late response.
    return;
  }

  enterprise_connectors::DeviceTrustResponse final_response = response;
  if (!final_response.error.has_value() &&
      final_response.challenge_response.empty()) {
    // An empty response with no error indicates an unexpected internal
    // failure, appropriately represented by kUnknown.
    final_response.error = enterprise_connectors::DeviceTrustError::kUnknown;
  }

  enterprise_connectors::LogDeviceTrustResponse(final_response,
                                                pending_request->start_time);
  if (!final_response.error.has_value()) {
    enterprise_connectors::LogAttestationFunnelStep(
        enterprise_connectors::DTAttestationFunnelStep::kChallengeResponseSent);
  }

  std::move(pending_request->callback).Run(final_response);
}

void DeviceTrustChallengeTabHelper::OnAttestationTimeout(RequestId request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto pending_request = TakePendingRequest(request_id);
  // The timer is owned by PendingRequest; if the request had already completed,
  // the timer would have been cancelled. Therefore, the pending request must
  // exist when the timeout callback runs.
  CHECK(pending_request);

  enterprise_connectors::DeviceTrustResponse response;
  response.error = enterprise_connectors::DeviceTrustError::kTimeout;
  enterprise_connectors::LogDeviceTrustResponse(response,
                                                pending_request->start_time);
  std::move(pending_request->callback).Run(response);

  // TODO(crbug.com/517885334): Track or cancel the underlying attestation
  // operation after its browser-side reply times out.
}

void DeviceTrustChallengeTabHelper::MaybeSetupDeviceTrustAPI(
    web::WebFrame* web_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_frame || !web_frame->IsMainFrame()) {
    return;
  }

  const url::Origin security_origin = web_frame->GetSecurityOrigin();
  if (security_origin.opaque()) {
    return;
  }

  if (!device_trust_service_ || !device_trust_service_->IsEnabled()) {
    return;
  }

  // WebFrame::GetUrl() is only used to refine the check within the frame's
  // security origin (for path-scoped allowlist patterns); a cross-origin URL
  // falls back to the security origin. This only controls whether the API is
  // exposed; the request-time check in BuildChallengeResponse() remains the
  // security boundary.
  const GURL policy_check_url =
      DeterminePolicyCheckUrl(security_origin, web_frame->GetUrl());
  if (device_trust_service_->Watches(policy_check_url).empty()) {
    return;
  }

  enterprise_connectors::LogAttestationFunnelStep(
      enterprise_connectors::DTAttestationFunnelStep::kAttestationFlowStarted);
  DeviceTrustJavaScriptFeature::GetInstance()->SetupDeviceTrustAPI(web_frame);
}

void DeviceTrustChallengeTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_context->HasCommitted() ||
      navigation_context->IsSameDocument()) {
    return;
  }

  // A committed cross-document navigation occurred. Invalidate in-flight
  // service callbacks for the old document and drop pending page replies and
  // their timers. Subsequent requests will obtain fresh weak pointers.
  weak_factory_.InvalidateWeakPtrs();
  pending_requests_.clear();

  // TODO(crbug.com/560094713): Track or cancel underlying service-level
  // operations across navigations when cancellation support is available.
}

void DeviceTrustChallengeTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  web_state_observation_.Reset();
  web_frames_manager_observation_.Reset();
  device_trust_service_ = nullptr;
  weak_factory_.InvalidateWeakPtrs();
  pending_requests_.clear();
}
