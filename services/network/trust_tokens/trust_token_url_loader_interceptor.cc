// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_url_loader_interceptor.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom.h"
#include "url/gurl.h"

namespace network {

namespace {

network::mojom::TrustTokenAccessDetailsPtr GetAccessDetails(
    mojom::TrustTokenOperationType operation,
    const GURL& request_url,
    const url::Origin& top_frame_origin,
    bool token_operation_unauthorized) {
  switch (operation) {
    case mojom::TrustTokenOperationType::kIssuance:
      return mojom::TrustTokenAccessDetails::NewIssuance(
          mojom::TrustTokenIssuanceDetails::New(
              top_frame_origin, url::Origin::Create(request_url),
              token_operation_unauthorized));
    case mojom::TrustTokenOperationType::kRedemption:
      return mojom::TrustTokenAccessDetails::NewRedemption(
          mojom::TrustTokenRedemptionDetails::New(
              top_frame_origin, url::Origin::Create(request_url),
              token_operation_unauthorized));
    case mojom::TrustTokenOperationType::kSigning:
      return mojom::TrustTokenAccessDetails::NewSigning(
          mojom::TrustTokenSigningDetails::New(top_frame_origin,
                                               token_operation_unauthorized));
  }
  NOTREACHED();
}

}  // namespace

// static
std::unique_ptr<TrustTokenUrlLoaderInterceptor>
TrustTokenUrlLoaderInterceptor::MaybeCreate(
    std::unique_ptr<TrustTokenRequestHelperFactory> factory) {
  if (!factory) {
    return nullptr;
  }
  return base::WrapUnique(
      new TrustTokenUrlLoaderInterceptor(std::move(factory)));
}

TrustTokenUrlLoaderInterceptor::TrustTokenUrlLoaderInterceptor(
    std::unique_ptr<TrustTokenRequestHelperFactory> factory)
    : factory_(std::move(factory)) {
  CHECK(factory_);
}

TrustTokenUrlLoaderInterceptor::~TrustTokenUrlLoaderInterceptor() = default;

int TrustTokenUrlLoaderInterceptor::GetAdditionalLoadFlags(
    const mojom::TrustTokenParams& params) const {
  // Trust token operations other than signing cannot be served from cache
  // because it needs to send the server the Trust Tokens request header and
  // get the corresponding response header. It is okay to cache the results in
  // case subsequent requests are made to the same URL in non-trust-token
  // settings.
  if (params.operation != mojom::TrustTokenOperationType::kSigning) {
    return net::LOAD_BYPASS_CACHE;
  }
  return 0;
}

void TrustTokenUrlLoaderInterceptor::BeginOperation(
    mojom::TrustTokenOperationType operation,
    const GURL& request_url,
    const url::Origin& top_frame_origin,
    const net::HttpRequestHeaders& headers,
    const mojom::TrustTokenParams& params,
    const net::NetLogWithSource& net_log,
    base::OnceCallback<mojom::TrustTokenAccessObserver*()> observer_getter,
    base::OnceCallback<
        base::OnceCallback<void(mojom::TrustTokenOperationResultPtr)>()>
        dev_tools_report_callback_getter,
    base::OnceCallback<
        void(base::expected<net::HttpRequestHeaders, net::Error>)> callback) {
  operation_ = operation;
  dev_tools_report_callback_getter_ =
      std::move(dev_tools_report_callback_getter);

  // Ask the factory to create the appropriate helper for the operation.
  // OnHelperCreated will continue the process once the helper is ready
  // (or if creation failed).
  factory_->CreateTrustTokenHelperForRequest(
      top_frame_origin, headers, params, net_log,
      base::BindOnce(&TrustTokenUrlLoaderInterceptor::OnHelperCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(observer_getter),
                     std::move(callback), std::cref(request_url),
                     top_frame_origin));
}

// Internal callback for when TrustTokenRequestHelperFactory finishes attempting
// to create the TrustTokenRequestHelper.
void TrustTokenUrlLoaderInterceptor::OnHelperCreated(
    base::OnceCallback<mojom::TrustTokenAccessObserver*()> observer_getter,
    base::OnceCallback<
        void(base::expected<net::HttpRequestHeaders, net::Error>)> callback,
    const GURL& request_url,
    const url::Origin& top_frame_origin,
    TrustTokenStatusOrRequestHelper status_or_helper) {
  // `operation_` must be set in BeginOperation().
  CHECK(operation_.has_value());
  // Notify the observer about the access attempt.
  if (auto* observer = std::move(observer_getter).Run()) {
    observer->OnTrustTokensAccessed(
        GetAccessDetails(*operation_, request_url, top_frame_origin,
                         status_or_helper.status() ==
                             mojom::TrustTokenOperationStatus::kUnauthorized));
  }

  // Handle failure to create the helper.
  if (!status_or_helper.ok()) {
    status_ = status_or_helper.status();
    CHECK(dev_tools_report_callback_getter_);
    if (auto dev_tools_report_callback =
            std::move(dev_tools_report_callback_getter_).Run()) {
      mojom::TrustTokenOperationResultPtr operation_result =
          mojom::TrustTokenOperationResult::New();
      operation_result->status = *status_;
      operation_result->operation = *operation_;
      std::move(dev_tools_report_callback).Run(std::move(operation_result));
    }
    // Inform the URLLoader that the operation failed.
    std::move(callback).Run(
        base::unexpected(net::ERR_TRUST_TOKEN_OPERATION_FAILED));
    return;
  }
  // Store the successfully created helper and proceed to the Begin step.
  helper_ = status_or_helper.TakeOrCrash();
  helper_->Begin(
      request_url,
      base::BindOnce(&TrustTokenUrlLoaderInterceptor::OnDoneBeginningOperation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TrustTokenUrlLoaderInterceptor::OnDoneBeginningOperation(
    base::OnceCallback<
        void(base::expected<net::HttpRequestHeaders, net::Error>)> callback,
    std::optional<net::HttpRequestHeaders> headers,
    mojom::TrustTokenOperationStatus status) {
  CHECK(operation_);
  // Store the status of the Begin operation. This might be updated later by
  // FinalizeOperation.
  status_ = status;

  // Record UMA for the outcome of the Begin step.
  base::UmaHistogramEnumeration(
      base::StrCat({"Net.TrustTokens.OperationOutcome.",
                    internal::TrustTokenOperationTypeToString(*operation_)}),
      status);

  // Handle success: pass the potentially modified headers back to URLLoader.
  if (status == mojom::TrustTokenOperationStatus::kOk) {
    DCHECK(headers);
    std::move(callback).Run(std::move(*headers));
    return;
  }
  // Handle failures or operations completed locally (without sending request).
  CHECK(!headers);

  // Report failure details to DevTools if possible. This happens here for
  // non-kOK statuses from Begin; for kOK, DevTools is reported after Finalize.
  MaybeSendTrustTokenOperationResultToDevTools();

  // When the Trust Tokens operation succeeded without needing to send the
  // request, we return early with an "error" representing this success.
  const net::Error err =
      status == mojom::TrustTokenOperationStatus::
                      kOperationSuccessfullyFulfilledLocally ||
              status == mojom::TrustTokenOperationStatus::kAlreadyExists
          ? net::ERR_TRUST_TOKEN_OPERATION_SUCCESS_WITHOUT_SENDING_REQUEST
          : net::ERR_TRUST_TOKEN_OPERATION_FAILED;
  std::move(callback).Run(base::unexpected(err));
}

void TrustTokenUrlLoaderInterceptor::FinalizeOperation(
    net::HttpResponseHeaders& response_headers,
    base::OnceCallback<void(net::Error)> callback) {
  // Finalize should only be called if the Begin step completed successfully
  // and required sending the request.
  CHECK(status_);
  CHECK_EQ(*status_, mojom::TrustTokenOperationStatus::kOk);
  CHECK(helper_);
  // Pass the response headers to the helper for processing.
  // OnDoneFinalizeOperation will handle the result.
  helper_->Finalize(
      response_headers,
      base::BindOnce(&TrustTokenUrlLoaderInterceptor::OnDoneFinalizeOperation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// Internal callback for when the TrustTokenRequestHelper finishes its Finalize
// operation.
void TrustTokenUrlLoaderInterceptor::OnDoneFinalizeOperation(
    base::OnceCallback<void(net::Error)> callback,
    mojom::TrustTokenOperationStatus status) {
  // Update the status to reflect the outcome of the Finalize step.
  status_ = status;

  // Report the final operation result (including the Finalize status) to
  // DevTools.
  MaybeSendTrustTokenOperationResultToDevTools();

  // Inform URLLoader whether the Finalize step succeeded (net::OK) or failed.
  std::move(callback).Run(status == mojom::TrustTokenOperationStatus::kOk
                              ? net::OK
                              : net::ERR_TRUST_TOKEN_OPERATION_FAILED);
}

// Helper to report the current operation's result to DevTools, if a callback
// was provided.
void TrustTokenUrlLoaderInterceptor::
    MaybeSendTrustTokenOperationResultToDevTools() {
  CHECK(dev_tools_report_callback_getter_);
  CHECK(helper_);
  CHECK(status_);
  if (auto dev_tools_report_callback =
          std::move(dev_tools_report_callback_getter_).Run()) {
    // Ask the helper to collect details based on the current status.
    std::move(dev_tools_report_callback)
        .Run(helper_->CollectOperationResultWithStatus(*status_));
  }
}

}  // namespace network
