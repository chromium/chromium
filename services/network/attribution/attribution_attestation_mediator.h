// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_ATTESTATION_MEDIATOR_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_ATTESTATION_MEDIATOR_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"

namespace network {

class TrustTokenKeyCommitmentGetter;

//  Class `AttributionAttestationMediator` handles a single trigger attestation
//  operation
//  (https://github.com/WICG/attribution-reporting-api/blob/main/trigger_attestation.md):
//  it generates a blind message using an underlying cryptographic library, asks
//  a private state token issuer to sign the blind message to obtain a blind
//  token, verifies and unblinds it using the cryptographic library to obtain a
//  token which is returned.
class AttributionAttestationMediator {
 public:
  // Represents the status/outcome of the execution of
  // `GetHeadersForAttestation`. These values are persisted to logs.
  enum class GetHeadersStatus {
    kSuccess = 0,
    kIssuerOriginNotSuitable = 1,
    kIssuerNotRegistered = 2,
    kUnableToInitializeCryptographer = 3,
    kUnableToAddKeysOnCryptographer = 4,
    kUnableToBlindMessage = 5,
    kMaxValue = kUnableToBlindMessage,
  };

  // Represents the status/outcome of the execution of
  // `ProcessAttestationToGetToken`. These values are persisted to logs.
  enum class ProcessAttestationStatus {
    kSuccess = 0,
    kNoSignatureReceivedFromIssuer = 1,
    kUnableToUnblindSignature = 2,
    kMaxValue = kUnableToUnblindSignature,
  };

  // Describe the ordered steps associated to completing an attestation
  // operation.
  enum class Step {
    kGetKeyCommitment = 0,
    kInitializeCryptographer = 1,
    kBlindMessage = 2,
    kSignBlindMessage = 3,
    kUnblindMessage = 4,
  };

  class Cryptographer {
   public:
    virtual ~Cryptographer() = default;

    // Initializes the Cryptographer. `issuer_configured_version` must be the
    // "protocol_version" value from the issuer's key commitment.
    //
    // Returns true on success and false if an internal error occurred in the
    // underlying cryptographic library.
    [[nodiscard]] virtual bool Initialize(
        mojom::TrustTokenProtocolVersion issuer_configured_version) = 0;

    // Stores a Private State Tokens issuance verification key for a subsequent
    // use verifying a blind token in `ConfirmIssuanceAndBeginRedemption`. May
    // be called multiple times to add multiple keys permissible for use during
    // this issuance.
    //
    // Returns true on success and false if the key is malformed or if an
    // internal error occurred in the underlying cryptographic library. Does not
    // forbid adding duplicates; however, duplicates might contribute to an
    // overall limit on the number of permitted keys, so the caller may wish to
    // ensure this is called at most once per distinct key.
    [[nodiscard]] virtual bool AddKey(base::StringPiece key) = 0;

    // On success, returns a base64-encoded string representing the blinded
    // `message`. on error, returns nullopt.
    //
    // `message` is a string representing the data that we want
    // to attest to. We "blind" it as part of the blind signature protocol
    // before sending it to the issuer for signature.
    [[nodiscard]] virtual absl::optional<std::string> BeginIssuance(
        base::StringPiece message) = 0;

    // Given a base64-encoded issuance `response header`, attempts to unblind a
    // blind token represented by the header using the keys previously added by
    // AddKey. If successful, it returns a token suitable for attachment in the
    // Sec-Attribution-Reporting-Private-State-Token header. On error, it
    // returns nullopt.
    [[nodiscard]] virtual absl::optional<std::string>
    ConfirmIssuanceAndBeginRedemption(base::StringPiece response_header) = 0;
  };

  class MetricsRecorder {
   public:
    virtual ~MetricsRecorder() = default;

    virtual void Start() = 0;
    virtual void Complete(Step step) = 0;
    virtual void FinishGetHeadersWith(GetHeadersStatus status) = 0;
    virtual void FinishProcessAttestationWith(
        ProcessAttestationStatus status) = 0;
  };

  static constexpr char kTriggerAttestationHeader[] =
      "Sec-Attribution-Reporting-Private-State-Token";

  AttributionAttestationMediator(
      const TrustTokenKeyCommitmentGetter* key_commitment_getter,
      std::unique_ptr<Cryptographer> cryptographer,
      std::unique_ptr<MetricsRecorder> metrics_recorder);
  ~AttributionAttestationMediator();

  AttributionAttestationMediator(const AttributionAttestationMediator&) =
      delete;
  AttributionAttestationMediator& operator=(
      const AttributionAttestationMediator&) = delete;

  // Returns headers used for attestation if the `url`'s origin is configured as
  // a private state tokens issuer.
  //
  // 1. Get the issuer's key commitment; if unavailable or unsuccessful, returns
  //    no headers
  // 2. Blind the message received; if unsuccessful, returns no headers
  // 3. Returns two headers;
  //    * Sec-Attribution-Reporting-Private-State-Token: with the blinded
  //      message.
  //    * Sec-Trust-Token-Version: with the protocol version configured in the
  //      issuers' key commitments
  //
  // `message` is a string representing the data that we want to attest to. The
  // `message` will be blinded before being sent to the issuer for signature.
  //
  // Later, when receiving the data with a token, the issuer will need to
  // re-generate this message to verify the token.
  void GetHeadersForAttestation(
      const GURL& url,
      const std::string& message,
      base::OnceCallback<void(net::HttpRequestHeaders)> done);

  // Process headers from an attestation response; if present and valid,
  // generates and returns a token that can be used for redemption.
  // 1. Checks `response_headers` for an attestation response header.
  // 2. If the header is present, strips it from `response_headers` and passes
  //    its value (blind token) to an underlying cryptographic library, which
  //    parses, validates and unblind the header to return a token.
  //
  // If both of these steps are successful, returns a token that can be send to
  // and verified by the issuer that signed the blind message.
  void ProcessAttestationToGetToken(
      net::HttpResponseHeaders& response_headers,
      base::OnceCallback<
          void(absl::optional<std::string> maybe_redemption_token)> done);

 private:
  struct CryptographerAndToken;
  struct CryptographerAndBlindMessage;

  // Continuation of `GetHeadersForAttestation` after asynchronous key
  // commitment fetching concludes. `done` is `GetHeadersForAttestation`'s
  // parameter, passed on to the continuation; `commitment_result` is the result
  // of the key commitment fetch.
  void OnGotKeyCommitment(
      base::OnceCallback<void(net::HttpRequestHeaders)> done,
      mojom::TrustTokenKeyCommitmentResultPtr commitment_result);

  // Continuation of `GetHeadersForAttestation` after an off-thread execution of
  // issuance operation (`Cryptographer::BeginIssuance`).`protocol_version` is
  // the private state issuer configured protocol versions; `done` is
  // `GetHeadersForAttestation`'s parameter, passed on to the continuation.
  // Receives ownership of the cryptographer back from the asynchronous callback
  // and stores it back in `cryptographer_` to reuse during
  // `ProcessAttestationToGetToken`.
  void OnDoneBeginIssuance(
      mojom::TrustTokenProtocolVersion protocol_version,
      base::OnceCallback<void(net::HttpRequestHeaders)> done,
      CryptographerAndBlindMessage cryptographer_and_blinded_token);

  // Continuation of `ProcessAttestationToGetToken` after an off-thread
  // execution to complete the issuance
  // (`Cryptographer::ConfirmIssuanceAndBeginRedemption`). `done` is
  // `ProcessAttestationToGetToken`'s parameter, passed on to the continuation.
  // Receives ownership of the cryptographer back from the asynchronous
  // callback.
  void OnDoneProcessingIssuanceResponse(
      base::OnceCallback<void(absl::optional<std::string>)> done,
      CryptographerAndToken cryptographer_and_redemption_token);

  // `message_` needs to be a nullable type because it is initialized in
  // `GetHeadersForAttestation`, but, once initialized, it will never be mutated
  // over the course of the operation's execution.
  absl::optional<std::string> message_;

  // The key_commitment_getter_ instance is a singleton owned by NetworkService,
  // it will always outlive this.
  const raw_ptr<const TrustTokenKeyCommitmentGetter> key_commitment_getter_;

  // Relinquishes ownership during posted tasks for the potentially
  // computationally intensive cryptographic operations
  // (Cryptographer::BeginIssuance,
  // Cryptographer::ConfirmIssuanceAndBeginRedemption); repopulated when
  // regaining ownership upon receiving each operation's results.
  std::unique_ptr<Cryptographer> cryptographer_;

  // The metrics recorder will be defined for the full lifecycle of this.
  std::unique_ptr<MetricsRecorder> metrics_recorder_;

  base::WeakPtrFactory<AttributionAttestationMediator> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_ATTESTATION_MEDIATOR_H_
