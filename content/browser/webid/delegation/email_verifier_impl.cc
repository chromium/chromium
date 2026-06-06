// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/email_verifier_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/email_verification_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"

namespace content::webid {

namespace {
const char kEmailVerifierKey[] = "kEmailVerifierKey";
}

EmailVerifierImpl::EmailVerifierImpl(RenderFrameHostImpl* render_frame_host)
    : request_builder_(base::BindRepeating(
          [](RenderFrameHostImpl* rfh) {
            return std::make_unique<EmailVerificationRequest>(*rfh);
          },
          render_frame_host)) {}

EmailVerifierImpl::EmailVerifierImpl(RequestBuilder builder)
    : request_builder_(std::move(builder)) {}

EmailVerifierImpl::~EmailVerifierImpl() = default;

void EmailVerifierImpl::Verify(
    const EmailVerifier::Result& result,
    const std::string& nonce,
    EmailVerifier::OnEmailVerifiedCallback callback) {
  std::unique_ptr<EmailVerificationRequest> request = request_builder_.Run();
  request->AddObserver(&performance_metrics_observer_);

  EmailVerificationRequest* request_ptr = request.get();
  request_ptr->Verify(result, nonce,
                      base::BindOnce(&EmailVerifierImpl::OnRequestComplete,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(request), std::move(callback)));
}

void EmailVerifierImpl::CheckIfVerifiable(
    const std::string& email,
    EmailVerifier::IsVerifiableCallback callback) {
  std::unique_ptr<EmailVerificationRequest> request = request_builder_.Run();
  request->AddObserver(&performance_metrics_observer_);

  EmailVerificationRequest* request_ptr = request.get();
  request_ptr->CheckIfVerifiable(
      email, base::BindOnce(
                 [](EmailVerifier::IsVerifiableCallback cb,
                    std::unique_ptr<EmailVerificationRequest> req,
                    std::optional<EmailVerifier::Result> result) {
                   std::move(cb).Run(std::move(result));
                 },
                 std::move(callback), std::move(request)));
}

void EmailVerifierImpl::OnRequestComplete(
    std::unique_ptr<EmailVerificationRequest> request,
    EmailVerifier::OnEmailVerifiedCallback callback,
    std::optional<std::string> result) {
  std::move(callback).Run(std::move(result));
}

EmailVerifierImpl::PerformanceMetricsObserver::PerformanceMetricsObserver() =
    default;
EmailVerifierImpl::PerformanceMetricsObserver::~PerformanceMetricsObserver() =
    default;

void EmailVerifierImpl::PerformanceMetricsObserver::OnIsVerifiableStart(
    EmailVerificationRequest* request) {
  is_verifiable_start_times_[request] = base::TimeTicks::Now();
}

void EmailVerifierImpl::PerformanceMetricsObserver::OnIsVerifiableComplete(
    EmailVerificationRequest* request,
    blink::mojom::EmailVerificationRequestResult status) {
  base::flat_map<EmailVerificationRequest*, base::TimeTicks>::iterator it =
      is_verifiable_start_times_.find(request);
  if (it != is_verifiable_start_times_.end()) {
    base::UmaHistogramMediumTimes("Blink.Evp.Timing.IsVerifiable",
                                  base::TimeTicks::Now() - it->second);
    is_verifiable_start_times_.erase(it);
  }
}

void EmailVerifierImpl::PerformanceMetricsObserver::OnVerifyStart(
    EmailVerificationRequest* request) {
  verify_start_times_[request] = base::TimeTicks::Now();
}

void EmailVerifierImpl::PerformanceMetricsObserver::OnVerifyComplete(
    EmailVerificationRequest* request,
    blink::mojom::EmailVerificationRequestResult status) {
  base::flat_map<EmailVerificationRequest*, base::TimeTicks>::iterator it =
      verify_start_times_.find(request);
  if (it != verify_start_times_.end()) {
    base::UmaHistogramMediumTimes("Blink.Evp.Timing.Verify",
                                  base::TimeTicks::Now() - it->second);
    verify_start_times_.erase(it);
  }
}

void EmailVerifierImpl::PerformanceMetricsObserver::OnRequestDestroyed(
    EmailVerificationRequest* request) {
  is_verifiable_start_times_.erase(request);
  verify_start_times_.erase(request);
}

// static
EmailVerifier* EmailVerifier::GetOrCreateForFrame(
    RenderFrameHost* render_frame_host) {
  auto* rfh = static_cast<RenderFrameHostImpl*>(render_frame_host);
  if (!rfh->GetUserData(kEmailVerifierKey)) {
    rfh->SetUserData(kEmailVerifierKey,
                     std::make_unique<EmailVerifierImpl>(rfh));
  }
  return static_cast<EmailVerifier*>(rfh->GetUserData(kEmailVerifierKey));
}

// static
void EmailVerifier::SetForFrameForTest(  // IN-TEST
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<EmailVerifier> verifier) {
  auto* rfh = static_cast<RenderFrameHostImpl*>(render_frame_host);
  rfh->SetUserData(kEmailVerifierKey, std::move(verifier));
}

}  // namespace content::webid
