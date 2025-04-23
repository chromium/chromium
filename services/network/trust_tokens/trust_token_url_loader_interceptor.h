// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_URL_LOADER_INTERCEPTOR_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_URL_LOADER_INTERCEPTOR_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/trust_tokens/trust_token_request_helper_factory.h"

class GURL;

namespace net {
class NetLogWithSource;
class HttpResponseHeaders;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace network {
namespace mojom {
class TrustTokenAccessObserver;
enum class TrustTokenOperationType;
class TrustTokenParams;
}  // namespace mojom

class TrustTokenRequestHelperFactory;

// Manages the Trust Token operations associated with a single URL request. It
// acts as an intermediary between URLLoader and the underlying
// TrustTokenRequestHelper.
//
// This class handles the two main phases of Trust Token interaction:
// 1. BeginOperation: Called before the network request is sent. It potentially
//    modifies request headers and determines if the request needs to be sent
//    at all (e.g., if the operation can be fulfilled locally).
// 2. FinalizeOperation: Called after response headers are received. It
//    processes response headers related to Trust Tokens.
//
// It interacts with a DevTools observer and reports UMA metrics.
class TrustTokenUrlLoaderInterceptor {
 public:
  // Factory method. Returns a interceptor if `factory` is non-null.
  static std::unique_ptr<TrustTokenUrlLoaderInterceptor> MaybeCreate(
      std::unique_ptr<TrustTokenRequestHelperFactory> factory);

  ~TrustTokenUrlLoaderInterceptor();

  TrustTokenUrlLoaderInterceptor(const TrustTokenUrlLoaderInterceptor&) =
      delete;
  TrustTokenUrlLoaderInterceptor& operator=(
      const TrustTokenUrlLoaderInterceptor&) = delete;

  // Returns additional net::LoadFlags required for the specified Trust Token
  // operation. Returns 0 if no additional flags are necessary.
  int GetAdditionalLoadFlags(const mojom::TrustTokenParams& params) const;

  // Starts the first phase of the Trust Token operation.
  // This attempts to create and run the `Begin` step of the appropriate
  // TrustTokenRequestHelper. `observer_getter` and
  // `dev_tools_report_callback_getter` provide callbacks for reporting.
  // `callback` is run with either the modified headers (on success) or a
  // net::Error code (on failure or the operation succeeded without needing to
  // send the request).
  void BeginOperation(
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
          void(base::expected<net::HttpRequestHeaders, net::Error>)> callback);

  // Starts the second phase of the Trust Token operation after receiving
  // response headers.
  // This runs the `Finalize` step of the TrustTokenRequestHelper.
  // `callback` is run with net::OK on success, or a net::Error code on failure.
  // Precondition: BeginOperation must have completed successfully (status() is
  // kOk).
  void FinalizeOperation(net::HttpResponseHeaders& response_headers,
                         base::OnceCallback<void(net::Error)> callback);

  // Returns the status of the last Trust Token operation step executed.
  const std::optional<mojom::TrustTokenOperationStatus>& status() const {
    return status_;
  }

 private:
  // `factory` must not be null.
  explicit TrustTokenUrlLoaderInterceptor(
      std::unique_ptr<TrustTokenRequestHelperFactory> factory);

  // Called after the TrustTokenRequestHelper is (or fails to be) created.
  void OnHelperCreated(
      base::OnceCallback<mojom::TrustTokenAccessObserver*()> observer_getter,
      base::OnceCallback<
          void(base::expected<net::HttpRequestHeaders, net::Error>)> callback,
      const GURL& request_url,
      const url::Origin& top_frame_origin,
      TrustTokenStatusOrRequestHelper status_or_helper);

  // Called after the TrustTokenRequestHelper's Begin operation completes.
  void OnDoneBeginningOperation(
      base::OnceCallback<
          void(base::expected<net::HttpRequestHeaders, net::Error>)> callback,
      std::optional<net::HttpRequestHeaders> headers,
      mojom::TrustTokenOperationStatus status);

  // Called after the TrustTokenRequestHelper's Finalize operation completes.
  void OnDoneFinalizeOperation(base::OnceCallback<void(net::Error)> callback,
                               mojom::TrustTokenOperationStatus status);

  // Helper to potentially send operation results to DevTools via the stored
  // callback getter.
  void MaybeSendTrustTokenOperationResultToDevTools();

  // Factory for creating the specific TrustTokenRequestHelper.
  std::unique_ptr<TrustTokenRequestHelperFactory> factory_;
  // The helper instance managing the current operation. Set in OnHelperCreated.
  std::unique_ptr<TrustTokenRequestHelper> helper_;
  // Type of the operation (issuance, redemption, signing). Set in
  // BeginOperation.
  std::optional<mojom::TrustTokenOperationType> operation_;
  // Retrieves the callback for reporting to DevTools.
  base::OnceCallback<
      base::OnceCallback<void(mojom::TrustTokenOperationResultPtr)>()>
      dev_tools_report_callback_getter_;
  // Stores the status of the last completed Trust Token step (Begin or
  // Finalize).
  std::optional<mojom::TrustTokenOperationStatus> status_;
  base::WeakPtrFactory<TrustTokenUrlLoaderInterceptor> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_URL_LOADER_INTERCEPTOR_H_
