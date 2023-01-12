// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_HELPER_FACTORY_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_HELPER_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "services/network/trust_tokens/trust_token_request_issuance_helper.h"

namespace network {

namespace internal {

// These are the possible results of constructing a Trust Tokens request helper;
// exposed in the header file for use in tests. Please do not use them directly
// outside of the class's implementation.
//
// Additionally, since this enum is used in histograms:
// 1. please do not reorder or delete values;
// 2. the values must be kept in sync with the enum of the same name in
// enums.xml.
enum class TrustTokenRequestHelperFactoryOutcome {
  kSuccessfullyCreatedAnIssuanceHelper = 0,
  kSuccessfullyCreatedARedemptionHelper = 1,
  kSuccessfullyCreatedASigningHelper = 2,
  kEmptyIssuersParameter = 3,
  kUnsuitableIssuerInIssuersParameter = 4,
  kUnsuitableTopFrameOrigin = 5,
  kRequestRejectedDueToBearingAnInternalTrustTokensHeader = 6,
  kRejectedByAuthorizer = 7,
  kMaxValue = kRejectedByAuthorizer
};

}  // namespace internal

class TrustTokenStatusOrRequestHelper;

// TrustTokenRequestHelperFactory dispatches a helper capable for executing a
// Trust Tokens (https://github.com/wicg/trust-token-api) operation against a
// single request.
class TrustTokenRequestHelperFactory {
 public:
  // Created helpers will use |store| to access persistent Trust
  // Tokens state and |key_commitment_getter| to obtain keys; consequently, both
  // arguments must outlive all of the created helpers.
  //
  // |context_client_provider| provides a handle to a NetworkContextClient that
  // will be used for requesting Trust Tokens operations' local execution.
  // context_client_provider.Run() will be called before each attempt to
  // delegate a Trust Tokens operation. It is permitted to return nullptr; in
  // this case, the operation will be cancelled.
  //
  // Each decision whether to vend a helper will first query |authorizer| to
  // determine whether it's currently allowed to execute Trust Tokens
  // operations.
  TrustTokenRequestHelperFactory(
      PendingTrustTokenStore* store,
      const TrustTokenKeyCommitmentGetter* key_commitment_getter,
      base::RepeatingCallback<mojom::NetworkContextClient*(void)>
          context_client_provider,
      base::RepeatingCallback<bool(void)> authorizer);

  TrustTokenRequestHelperFactory(const TrustTokenRequestHelperFactory&) =
      delete;
  TrustTokenRequestHelperFactory& operator=(
      const TrustTokenRequestHelperFactory&) = delete;

  virtual ~TrustTokenRequestHelperFactory();

  // Attempts to create a TrustTokenRequestHelper able to help execute the Trust
  // Tokens protocol operation given by |params| against the request |request|.
  //
  // If |request| contains any Trust Tokens request headers (see
  // trust_token_http_headers.h), or if |request|'s top frame origin is missing
  // or not both (1) potentially trustworthy and (2) either HTTP or HTTPS,
  // returns kInvalidArgument.
  //
  // If the corresponding Trust Tokens operation is not yet implemented, returns
  // kUnavailable.
  //
  // On success, returns kOk alongside a request helper corresponding to
  // |request|'s Trust Tokens parameters, using |store| to access persistent
  // state.
  virtual void CreateTrustTokenHelperForRequest(
      const url::Origin& top_frame_origin,
      const net::HttpRequestHeaders& headers,
      const mojom::TrustTokenParams& params,
      const net::NetLogWithSource& net_log,
      base::OnceCallback<void(TrustTokenStatusOrRequestHelper)> done);

 private:
  // Continuation of |CreateTrustTokenHelperForRequest|. Uses |store|, alongside
  // the information provided to |CreateTrustTokenHelperForRequest|, to finish
  // constructing a store or return an error.
  void ConstructHelperUsingStore(
      SuitableTrustTokenOrigin top_frame_origin,
      mojom::TrustTokenParamsPtr params,
      net::NetLogWithSource net_log,
      base::OnceCallback<void(TrustTokenStatusOrRequestHelper)> done,
      TrustTokenStore* store);

  raw_ptr<PendingTrustTokenStore> store_;
  raw_ptr<const TrustTokenKeyCommitmentGetter> key_commitment_getter_;
  base::RepeatingCallback<mojom::NetworkContextClient*(void)>
      context_client_provider_;
  base::RepeatingCallback<bool(void)> authorizer_;

  base::WeakPtrFactory<TrustTokenRequestHelperFactory> weak_factory_{this};
};

class TrustTokenStatusOrRequestHelper {
 public:
  TrustTokenStatusOrRequestHelper();

  // Deliberately allow implicit conversion because the object
  // "is" the status (or the helper).
  //
  // |status| must not be kOk. (In case of success, construct the
  // StatusOrRequestHelper by passing a helper.)
  TrustTokenStatusOrRequestHelper(  // NOLINT
      mojom::TrustTokenOperationStatus status);

  // Sets the stored status to kOk and the stored helper to |helper|. |helper|
  // must not be null.
  TrustTokenStatusOrRequestHelper(  // NOLINT
      std::unique_ptr<TrustTokenRequestHelper> helper);

  ~TrustTokenStatusOrRequestHelper();

  TrustTokenStatusOrRequestHelper(TrustTokenStatusOrRequestHelper&&);
  TrustTokenStatusOrRequestHelper& operator=(TrustTokenStatusOrRequestHelper&&);

  bool ok() const { return status_ == mojom::TrustTokenOperationStatus::kOk; }
  operator mojom::TrustTokenOperationStatus() const {  // NOLINT
    return status_;
  }
  mojom::TrustTokenOperationStatus status() const { return status_; }

  std::unique_ptr<TrustTokenRequestHelper> TakeOrCrash() {
    CHECK_EQ(status_, mojom::TrustTokenOperationStatus::kOk);
    return std::move(helper_);
  }

 private:
  mojom::TrustTokenOperationStatus status_ =
      mojom::TrustTokenOperationStatus::kOk;
  std::unique_ptr<TrustTokenRequestHelper> helper_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_HELPER_FACTORY_H_
