// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_SIGNING_HELPER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_SIGNING_HELPER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "url/origin.h"

namespace network {

class TrustTokenStore;

// Class TrustTokenRequestSigningHelper executes a single trust token signing
// operation (https://github.com/wicg/trust-token-api): it searches storage for
// a Redemption Record (RR) and attaches the RR to the request.
class TrustTokenRequestSigningHelper : public TrustTokenRequestHelper {
 public:
  struct Params {
    // Refer to fields' comments for their semantics.
    Params(std::vector<SuitableTrustTokenOrigin> issuers,
           SuitableTrustTokenOrigin toplevel);

    // Minimal convenience constructor. Other fields have reasonable defaults,
    // but it's necessary to have |issuer| and |toplevel| at construction time
    // since SuitableTrustTokenOrigin has no default constructor.
    Params(SuitableTrustTokenOrigin issuer, SuitableTrustTokenOrigin toplevel);
    ~Params();

    Params(const Params&);
    Params& operator=(const Params&);
    Params(Params&&);
    Params& operator=(Params&&);

    // |issuers| contains the Trust Tokens issuer origins for which to retrieve
    // Redemption Records and matching signing keys. These must be both
    // (1) HTTP or HTTPS and (2) "potentially trustworthy". This precondition is
    // slightly involved because there are two needs:
    //   1. HTTP or HTTPS so that the scheme serializes in a sensible manner in
    //   order to serve as a key for persisting state,
    //   2. potentially trustworthy to satisfy Web security requirements.
    std::vector<SuitableTrustTokenOrigin> issuers;

    // |toplevel| is the top-level origin of the initiating request. This must
    // satisfy the same preconditions as |issuer|.
    SuitableTrustTokenOrigin toplevel;
  };

  // Creates a request signing helper with behavior determined by |params|,
  // relying on |token_store| to provide protocol state.
  //
  // |token_store| must outlive this object.
  TrustTokenRequestSigningHelper(
      TrustTokenStore* token_store,
      Params params,
      net::NetLogWithSource net_log = net::NetLogWithSource());

  ~TrustTokenRequestSigningHelper() override;

  TrustTokenRequestSigningHelper(const TrustTokenRequestSigningHelper&) =
      delete;
  TrustTokenRequestSigningHelper& operator=(
      const TrustTokenRequestSigningHelper&) = delete;

  // Attempts to attach Redemption Records (RRs) corresponding to request's
  // initiating top-level origin and the provided issuer origins.
  //
  // ATTACHING THE REDEMPTION RECORD:
  // In the case that an RR is found for at least one provided issuer and the
  // requested headers to sign are well-formed, attaches a
  // Sec-Redemption-Record header bearing the RRs.
  //
  // FAILS IF:
  // 1. none of the provided issuers has an RR corresponding to this top-level
  // origin in |token_store_|; or
  // 2. an internal error occurs during header serialization.
  //
  // POSTCONDITIONS:
  // - Always returns kOk. This is to avoid aborting a request entirely due to a
  // failure during signing; see the Trust Tokens design doc for more
  // discussion.
  // - On failure, the request will contain an empty
  // Sec-Redemption-Record header.
  void Begin(
      const GURL& url,
      base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                              mojom::TrustTokenOperationStatus)> done) override;

  // Immediately returns kOk with no other effect. (Signing is an operation that
  // only needs to process requests, not their corresponding responses.)
  void Finalize(
      net::HttpResponseHeaders& response_headers,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) override;

  mojom::TrustTokenOperationResultPtr CollectOperationResultWithStatus(
      mojom::TrustTokenOperationStatus status) override;

 private:
  raw_ptr<TrustTokenStore> token_store_;

  Params params_;

  net::NetLogWithSource net_log_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_SIGNING_HELPER_H_
