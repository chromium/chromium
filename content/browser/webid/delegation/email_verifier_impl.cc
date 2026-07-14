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

// `PerformanceMetricsObserver` measures timing metrics for a single request.
// It is instantiated per-request (inside `CheckIfVerifiable` or `Verify`) and
// its ownership is bound to the asynchronous completion callback passed to the
// request. Because the request holds its completion callback until completion,
// the observer's lifetime is guaranteed to match the request execution
// lifetime. It unregisters itself upon destruction, or nulls out `request_` if
// the request is destroyed early.
class PerformanceMetricsObserver : public EmailVerificationRequest::Observer {
 public:
  explicit PerformanceMetricsObserver(EmailVerificationRequest* request)
      : request_(request) {
    request_->AddObserver(this);
  }

  ~PerformanceMetricsObserver() override {
    if (request_) {
      request_->RemoveObserver(this);
    }
  }

  void OnIsVerifiableStart() override {
    is_verifiable_start_time_ = base::TimeTicks::Now();
  }

  void OnIsVerifiableComplete(
      blink::mojom::EmailVerificationRequestResult status) override {
    CHECK(!is_verifiable_start_time_.is_null());
    base::UmaHistogramMediumTimes(
        "Blink.Evp.Timing.IsVerifiable",
        base::TimeTicks::Now() - is_verifiable_start_time_);
    is_verifiable_start_time_ = base::TimeTicks();
  }

  void OnVerifyStart() override { verify_start_time_ = base::TimeTicks::Now(); }

  void OnVerifyComplete(
      blink::mojom::EmailVerificationRequestResult status) override {
    CHECK(!verify_start_time_.is_null());
    base::UmaHistogramMediumTimes("Blink.Evp.Timing.Verify",
                                  base::TimeTicks::Now() - verify_start_time_);
    verify_start_time_ = base::TimeTicks();
  }

  void OnRequestDestroyed() override { request_ = nullptr; }

 private:
  // Non-owning pointer to the request being observed. Ownership of the
  // request is held by the callback parameter tuple.
  raw_ptr<EmailVerificationRequest> request_;
  base::TimeTicks is_verifiable_start_time_;
  base::TimeTicks verify_start_time_;
};

}  // namespace

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
  std::unique_ptr<PerformanceMetricsObserver> observer =
      std::make_unique<PerformanceMetricsObserver>(request.get());

  EmailVerificationRequest* request_ptr = request.get();
  request_ptr->Verify(
      result, nonce,
      base::BindOnce(&EmailVerifierImpl::OnRequestComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(callback), std::move(observer)));
}

void EmailVerifierImpl::CheckIfVerifiable(
    const std::string& email,
    EmailVerifier::IsVerifiableCallback callback) {
  std::unique_ptr<EmailVerificationRequest> request = request_builder_.Run();
  std::unique_ptr<PerformanceMetricsObserver> observer =
      std::make_unique<PerformanceMetricsObserver>(request.get());

  EmailVerificationRequest* request_ptr = request.get();
  request_ptr->CheckIfVerifiable(
      email, base::BindOnce(
                 [](EmailVerifier::IsVerifiableCallback cb,
                    std::unique_ptr<EmailVerificationRequest> req,
                    std::unique_ptr<EmailVerificationRequest::Observer> obs,
                    std::optional<EmailVerifier::Result> result) {
                   std::move(cb).Run(std::move(result));
                 },
                 std::move(callback), std::move(request), std::move(observer)));
}

void EmailVerifierImpl::OnRequestComplete(
    std::unique_ptr<EmailVerificationRequest> request,
    EmailVerifier::OnEmailVerifiedCallback callback,
    std::unique_ptr<EmailVerificationRequest::Observer> observer,
    std::optional<std::string> result) {
  std::move(callback).Run(std::move(result));
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
    RenderFrameHost* render_frame_host,
    std::unique_ptr<EmailVerifier> verifier) {
  auto* rfh = static_cast<RenderFrameHostImpl*>(render_frame_host);
  rfh->SetUserData(kEmailVerifierKey, std::move(verifier));
}

}  // namespace content::webid
