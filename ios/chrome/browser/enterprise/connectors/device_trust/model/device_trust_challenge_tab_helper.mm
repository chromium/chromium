// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_challenge_tab_helper.h"

#import <utility>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/task/sequenced_task_runner.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_java_script_feature.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {
// Browser-side timeout for device attestation requests.
constexpr base::TimeDelta kAttestationTimeout = base::Seconds(25);
}  // namespace

DeviceTrustChallengeTabHelper::PendingRequest::PendingRequest(
    AttestationCallback callback)
    : callback(std::move(callback)) {}

DeviceTrustChallengeTabHelper::PendingRequest::~PendingRequest() = default;

DeviceTrustChallengeTabHelper::DeviceTrustChallengeTabHelper(
    web::WebState* web_state)
    : web_state_(CHECK_DEREF(web_state)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  enterprise_connectors::DeviceTrustService* service =
      DeviceTrustServiceFactoryIOS::GetForProfile(profile);

  if (!service || !service->IsEnabled()) {
    PostError(std::move(callback),
              enterprise_connectors::DeviceTrustError::kServiceUnavailable);
    return;
  }

  // Opaque origins have no authority to request device attestation.
  // TODO(crbug.com/563331507): Return a specific error code (e.g.
  // kInvalidOrigin) instead of kUnknown.
  if (security_origin.opaque()) {
    PostError(std::move(callback),
              enterprise_connectors::DeviceTrustError::kUnknown);
    return;
  }

  // Determine the URL to evaluate against Device Trust allowlist policies.
  // By default, use the security origin's base URL. If a valid request URL is
  // present and matches the security origin, prefer the request URL so that
  // path-scoped allowlist patterns (e.g. "https://example.com/login") can
  // match.
  GURL policy_check_url = security_origin.GetURL();
  if (request_url.has_value() && request_url->is_valid()) {
    if (url::Origin::Create(*request_url) == security_origin) {
      policy_check_url = *request_url;
    }
  }

  const std::set<enterprise_connectors::DTCPolicyLevel> levels =
      service->Watches(policy_check_url);
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

  const RequestId request_id{next_request_id_++};
  auto request = std::make_unique<PendingRequest>(std::move(callback));
  request->timer.Start(
      FROM_HERE, kAttestationTimeout,
      base::BindOnce(&DeviceTrustChallengeTabHelper::OnAttestationTimeout,
                     weak_factory_.GetWeakPtr(), request_id));
  pending_requests_.emplace(request_id, std::move(request));

  service->BuildChallengeResponse(
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
    // Request was already taken (e.g. timed out). Ignore late response.
    return;
  }

  if (response.error.has_value()) {
    std::move(pending_request->callback).Run(response);
    return;
  }

  if (response.challenge_response.empty()) {
    // An empty response with no error indicates an unexpected internal
    // failure, appropriately represented by kUnknown.
    enterprise_connectors::DeviceTrustResponse failed_response = response;
    failed_response.error = enterprise_connectors::DeviceTrustError::kUnknown;
    std::move(pending_request->callback).Run(failed_response);
    return;
  }

  std::move(pending_request->callback).Run(response);
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

  // TODO(crbug.com/517112324): Check DeviceTrustService::Watches(url).
  DeviceTrustJavaScriptFeature::GetInstance()->SetupDeviceTrustAPI(web_frame);
}

void DeviceTrustChallengeTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  web_state_observation_.Reset();
  web_frames_manager_observation_.Reset();
  weak_factory_.InvalidateWeakPtrs();
  pending_requests_.clear();
}
