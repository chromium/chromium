// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_ISSUANCE_HELPER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_ISSUANCE_HELPER_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "url/origin.h"

namespace network {
class TrustTokenStore;

// Class TrustTokenRequestIssuanceHelper handles a single trust token issuance
// operation (https://github.com/wicg/trust-token-api): it generates blinded,
// unsigned tokens using an underlying cryptographic library, asks a token
// issuer to sign the tokens, verifies the result, and unblinds and stores the
// tokens. The normal case involves a total of two network requests: one to get
// an up-to-date view of a key set the issuer provides for verifying its
// signatures, and another to send blinded tokens to the issuer.
class TrustTokenRequestIssuanceHelper : public TrustTokenRequestHelper {
 public:
  // Class Cryptographer executes the underlying cryptographic
  // operations required for issuance. The API is intended to correspond closely
  // to the BoringSSL API.
  //
  // Usage:
  //   1. 1x successful Initialize (without a success, don't call AddKey), then
  //   2. >= 1x successful AddKey (without at least one success, don't call
  //   BeginIssuance), then
  //   3. 1x successful BeginIssuance (without a success, don't call
  //   ConfirmIssuance), then
  //   4. 1x ConfirmIssuance.
  class Cryptographer {
   public:
    virtual ~Cryptographer() = default;

    // Initializes the delegate. |issuer_configured_version| and
    // |issuer_configured_batch_size| must be the "protocol_version" and
    // "batchsize" values from an issuer-provided key commitment result.
    //
    // Returns true on success and false if the batch size is unacceptable or an
    // internal error occurred in the underlying cryptographic library.
    virtual bool Initialize(
        mojom::TrustTokenProtocolVersion issuer_configured_version,
        int issuer_configured_batch_size) = 0;

    // Stores a Trust Tokens issuance verification key for subsequent use
    // verifying signed tokens in |ConfirmIssuance|. May be called multiple
    // times to add multiple keys permissible for use during this issuance.
    // (Typically, an issuer will have around three simultaneously active public
    // keys.)
    //
    // Returns true on success and false if the key is malformed or if an
    // internal error occurred in the underlying cryptographic library. Does not
    // forbid adding duplicates; however, duplicates might contribute to an
    // overall limit on the number of permitted keys, so the caller may wish to
    // ensure this is called at most once per distinct key.
    virtual bool AddKey(std::string_view key) = 0;

    // On success, returns a base64-encoded string representing |num_tokens|
    // many blinded, unsigned trust tokens; on error, returns nullopt. The
    // format of this string will eventually be specified, but it is currently
    // considered an implementation detail of the underlying cryptographic code.
    virtual std::optional<std::string> BeginIssuance(size_t num_tokens) = 0;

    struct UnblindedTokens {
      UnblindedTokens();
      ~UnblindedTokens();

      std::vector<std::string> tokens;
      // The verification key that led to the tokens' successful verification.
      // This will be associated with the tokens in persistent storage and,
      // subsequently, used to determine whether the tokens are still alive.
      std::string body_of_verifying_key;
    };

    // Given a base64-encoded issuance response header, attempts to unblind
    // the tokens represented by the header using the keys previously added by
    // AddKey. If successful, returns a vector of tokens (as bytestrings), along
    // with the key (from the issuer's key commitment registry) that
    // successfully validated the signed tokens. Otherwise, returns nullptr.
    virtual std::unique_ptr<UnblindedTokens> ConfirmIssuance(
        std::string_view response_header) = 0;
  };

  // Creates a new issuance helper.
  //
  // - |top_level_origin| is the top-level origin of the request subsequently
  //   passed to Begin; |top_level_origin|'s scheme must be both (1) HTTP or
  //   HTTPS and (2) "potentially trustworthy". This precondition is slightly
  //   involved because there are two needs:
  //   1. HTTP or HTTPS so that the scheme serializes in a sensible manner in
  //      order to serve as a key for persisting state.
  //   2. potentially trustworthy origin to satisfy Web security requirements.
  // - |token_store| will be responsible for storing underlying Trust Tokens
  // state. It must outlive this object.
  // - |key_commitment_getter|, |cryptographer|, and |local_operation_delegate|
  // are delegates that help execute the protocol; see their class comments.
  // They must live outlive this object.
  // - |is_current_os_callback| should return whether, when the given Os is
  // present in the issuer's "request_issuance_locally_on" field in its key
  // commitment, the issuance helper should attempt to forward requests to the
  // local operation delegate.
  // - |metrics_delegate|, which must outlive this object, will learn about
  // certain aspects of this operation's execution in order to slice the metrics
  // that it reports.
  //
  // REQUIRES: |token_store|, |key_commitment_getter|, |cryptographer|,
  // |local_operation_delegate|, |is_current_os_callback|, and
  // |metrics_delegate|  must be non-null.
  TrustTokenRequestIssuanceHelper(
      SuitableTrustTokenOrigin top_level_origin,
      TrustTokenStore* token_store,
      const TrustTokenKeyCommitmentGetter* key_commitment_getter,
      std::optional<std::string> custom_key_commitment,
      std::optional<url::Origin> custom_issuer,
      std::unique_ptr<Cryptographer> cryptographer,
      net::NetLogWithSource net_log = net::NetLogWithSource());
  ~TrustTokenRequestIssuanceHelper() override;

  // Executes the outbound part of a Trust Tokens issuance operation,
  // interpreting |url|'s origin as the token issuer origin;
  // 1. Checks preconditions (see "Returns" below); if unsuccessful, fails
  // 2. Executes a Trust Tokens key commitment request against the issuer; if
  //    unsuccessful, fails
  // 3. Returns headers containing a number of signed, unblinded tokens equal
  //    to the lesser of:
  //    * the issuer's configured batch size
  //    * a fixed limit on maximum number of tokens to send per request (see
  //      trust_token_parameterization.h).
  //
  // Returns:
  // * kOk on success
  // * kInternalError if generating blinded, unsigned tokens fails (this is a
  //   cryptographic operation not depending on protocol state)
  // * kResourceExhausted if there is no space to store more tokens
  //   corresponding to this issuer, or if the top-level origin provided to this
  //   object's constructor has already reached its number-of-issuers limit
  // * kMissingIssuerKeys if there are no valid keys for the issuer
  // * kFailedPrecondition if preconditions fail, including receiving a
  //   malformed or otherwise invalid key commitment record from the issuer
  //
  // The |top_level_origin_|, and its destination |url|'s origin, must be both
  // (1) HTTP or HTTPS and (2) "potentially trustworthy" in the sense of
  // network::IsOriginPotentiallyTrustworthy. (See the justification in the
  // constructor's comment.)
  void Begin(
      const GURL& url,
      base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                              mojom::TrustTokenOperationStatus)> done) override;

  // Performs the second half of Trust Token issuance's client side,
  // corresponding to the "To process an issuance response" section in the
  // normative pseudocode:
  // 1. Checks |response_headers| for an issuance response header.
  // 2. If the header is present, strips it from |response_headers| and passes
  // its value to an underlying cryptographic library, which parses and
  // validates the header and splits it into a number of signed, unblinded
  // tokens.
  //
  // If both of these steps are successful, stores the tokens in |token_store_|
  // and returns kOk. Otherwise, returns kBadResponse.
  void Finalize(
      net::HttpResponseHeaders& response_headers,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) override;

  // These internal structs are in the public namespace so that
  // anonymous-namespace functions in the .cc file can construct them.
  struct CryptographerAndBlindedTokens;
  struct CryptographerAndUnblindedTokens;

  mojom::TrustTokenOperationResultPtr CollectOperationResultWithStatus(
      mojom::TrustTokenOperationStatus status) override;

 private:
  // Continuation of |Begin| after asynchronous key commitment fetching
  // concludes.
  //
  // |request| and |done| are |Begin|'s parameters, passed on to the
  // continuation; |commitment_result| is the result of the key commitment
  // fetch.
  void OnGotKeyCommitment(
      const GURL& url,
      base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                              mojom::TrustTokenOperationStatus)> done,
      mojom::TrustTokenKeyCommitmentResultPtr commitment_result);

  // Continuation of |Begin| after a call to the cryptography delegate to
  // execute the bulk of the outbound half of the issuance operation. Receives
  // ownership of the cryptographer back from the asynchronous callback and
  // should store the cryptographer back in |cryptographer_| to reuse during
  // |Finalize|.
  void OnDelegateBeginIssuanceCallComplete(
      const GURL& url,
      base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                              mojom::TrustTokenOperationStatus)> done,
      CryptographerAndBlindedTokens cryptographer_and_blinded_tokens);

  // Continuation of |Finalize| after extracting the base64-encoded issuance
  // response from a response header (or receiving it from a locally executed
  // operation).
  void ProcessIssuanceResponse(
      std::string issuance_response,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done);

  // Continuation of |Finalize| after processing the received issuance response,
  // which typically involves an off-thread call to the cryptography delegate to
  // execute the bulk of the inbound half of the issuance operation.
  // Receives ownership of the cryptographer back from the asynchronous
  // callback.
  void OnDoneProcessingIssuanceResponse(
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
      CryptographerAndUnblindedTokens cryptographer_and_unblinded_tokens);

  // |issuer_| needs to be a nullable type because it is initialized in |Begin|,
  // but, once initialized, it will never be empty over the course of the
  // operation's execution.
  std::optional<SuitableTrustTokenOrigin> issuer_;
  const SuitableTrustTokenOrigin top_level_origin_;
  const raw_ptr<TrustTokenStore> token_store_;
  const raw_ptr<const TrustTokenKeyCommitmentGetter> key_commitment_getter_;
  const std::optional<std::string> custom_key_commitment_;
  const std::optional<url::Origin> custom_issuer_;

  mojom::TrustTokenProtocolVersion protocol_version_;

  // Relinquishes ownership during posted tasks for the potentially
  // computationally intensive cryptographic operations
  // (Cryptographer::BeginIssuance, Cryptographer::ConfirmIssuance); repopulated
  // when regaining ownership upon receiving each operation's results.
  std::unique_ptr<Cryptographer> cryptographer_;

  net::NetLogWithSource net_log_;
  std::optional<size_t> num_obtained_tokens_;
  base::WeakPtrFactory<TrustTokenRequestIssuanceHelper> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_ISSUANCE_HELPER_H_
