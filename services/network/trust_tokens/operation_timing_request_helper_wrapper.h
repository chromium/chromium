// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_OPERATION_TIMING_REQUEST_HELPER_WRAPPER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_OPERATION_TIMING_REQUEST_HELPER_WRAPPER_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "services/network/trust_tokens/trust_token_operation_metrics_recorder.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "services/network/trust_tokens/trust_token_request_issuance_helper.h"

namespace network {

// OperationTimingRequestHelperWrapper passes a Trust Tokens operation through
// to a TrustTokenRequestHelper responsible for executing the operation and
// records metrics timing the operation.
class OperationTimingRequestHelperWrapper : public TrustTokenRequestHelper {
 public:
  explicit OperationTimingRequestHelperWrapper(
      std::unique_ptr<TrustTokenOperationMetricsRecorder> metrics_recorder,
      std::unique_ptr<TrustTokenRequestHelper> helper);
  ~OperationTimingRequestHelperWrapper() override;

  // TrustTokenRequestHelper implementation:
  void Begin(
      const GURL& url,
      base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                              mojom::TrustTokenOperationStatus)> done) override;

  void Finalize(
      net::HttpResponseHeaders& response_headers,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) override;

  mojom::TrustTokenOperationResultPtr CollectOperationResultWithStatus(
      mojom::TrustTokenOperationStatus status) override;

 private:
  // Records timing metrics, then calls the callback.
  void FinishBegin(
      base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                              mojom::TrustTokenOperationStatus)> done,
      std::optional<net::HttpRequestHeaders> request_headers,
      mojom::TrustTokenOperationStatus status);

  // Records timing metrics, then calls the callback.
  void FinishFinalize(
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
      mojom::TrustTokenOperationStatus status);

  std::unique_ptr<TrustTokenOperationMetricsRecorder> recorder_;
  std::unique_ptr<TrustTokenRequestHelper> helper_;

  base::WeakPtrFactory<OperationTimingRequestHelperWrapper> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_OPERATION_TIMING_REQUEST_HELPER_WRAPPER_H_
