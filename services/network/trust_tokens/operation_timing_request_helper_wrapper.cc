// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/operation_timing_request_helper_wrapper.h"

#include "net/http/http_request_headers.h"

namespace network {

OperationTimingRequestHelperWrapper::OperationTimingRequestHelperWrapper(
    std::unique_ptr<TrustTokenOperationMetricsRecorder> metrics_recorder,
    std::unique_ptr<TrustTokenRequestHelper> helper)
    : recorder_(std::move(metrics_recorder)), helper_(std::move(helper)) {}

OperationTimingRequestHelperWrapper::~OperationTimingRequestHelperWrapper() =
    default;

void OperationTimingRequestHelperWrapper::Begin(
    const GURL& url,
    base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                            mojom::TrustTokenOperationStatus)> done) {
  recorder_->BeginBegin();
  helper_->Begin(
      url, base::BindOnce(&OperationTimingRequestHelperWrapper::FinishBegin,
                          weak_factory_.GetWeakPtr(), std::move(done)));
}

void OperationTimingRequestHelperWrapper::Finalize(
    net::HttpResponseHeaders& response_headers,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  recorder_->BeginFinalize();
  helper_->Finalize(
      response_headers,
      base::BindOnce(&OperationTimingRequestHelperWrapper::FinishFinalize,
                     weak_factory_.GetWeakPtr(), std::move(done)));
}

void OperationTimingRequestHelperWrapper::FinishBegin(
    base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                            mojom::TrustTokenOperationStatus)> done,
    std::optional<net::HttpRequestHeaders> request_headers,
    mojom::TrustTokenOperationStatus status) {
  recorder_->FinishBegin(status);
  std::move(done).Run(std::move(request_headers), status);
}

void OperationTimingRequestHelperWrapper::FinishFinalize(
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    mojom::TrustTokenOperationStatus status) {
  recorder_->FinishFinalize(status);
  std::move(done).Run(status);
}

mojom::TrustTokenOperationResultPtr
OperationTimingRequestHelperWrapper::CollectOperationResultWithStatus(
    mojom::TrustTokenOperationStatus status) {
  return helper_->CollectOperationResultWithStatus(status);
}

}  // namespace network
