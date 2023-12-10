// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_VERIFICATION_MEDIATOR_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_VERIFICATION_MEDIATOR_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"

namespace network {

class TrustTokenKeyCommitmentGetter;

//  Class `AttributionVerificationMediator` handles a single report verification
//  operation
//  (https://github.com/WICG/attribution-reporting-api/blob/main/report_verification.md):
//  it generates a blind message using an underlying cryptographic library, asks
//  a private state token issuer to sign the blind message to obtain a blind
//  token, verifies and unblinds it using the cryptographic library to obtain a
//  token which is returned.
class AttributionVerificationMediator {
 public:
  using Message = std::string;
  using BlindedMessage = std::string;
  using BlindedToken = std::string;
  using Token = std::string;

  // Represents the status/outcome of the execution of
  // `GetHeadersForVerification`. These values are persisted to logs.
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
  // `ProcessVerificationToGetTokens`. These values are persisted to logs.
  enum class ProcessVerificationStatus {
    kSuccess = 0,
    kNoSignatureReceivedFromIssuer = 1,
    kUnableToUnblindSignature = 2,
    kTooManySignaturesReceivedFromIssuer = 3,
    kBadSignaturesHeaderReceivedFromIssuer = 4,
    kMaxValue = kBadSignaturesHeaderReceivedFromIssuer,
  };

  // Describe the ordered steps associated to completing a verification
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
    [[nodiscard]] virtual bool AddKey(std::string_view key) = 0;

    // On success, returns a base64-encoded string representing the blinded
    // `message`. on error, returns nullopt.
    //
    // `message` is a string representing the data that we want
    // to attest to. We "blind" it as part of the blind signature protocol
    // before sending it to the issuer for signature.
    [[nodiscard]] virtual absl::optional<std::string> BeginIssuance(
        std::string_view message) = 0;

    // Given a base64-encoded issuance `response header`, attempts to unblind a
    // blind token represented by the header using the keys previously added by
    // AddKey. If successful, it returns a token suitable for attachment in the
    // Sec-Attribution-Reporting-Private-State-Token header. On error, it
    // returns nullopt.
    [[nodiscard]] virtual absl::optional<std::string>
    ConfirmIssuanceAndBeginRedemption(std::string_view response_header) = 0;
  };

  class MetricsRecorder {
   public:
    virtual ~MetricsRecorder() = default;

    virtual void Start() = 0;
    virtual void Complete(Step step) = 0;
    virtual void FinishGetHeadersWith(GetHeadersStatus status) = 0;
    virtual void FinishProcessVerificationWith(
        ProcessVerificationStatus status) = 0;
  };

  static constexpr char kReportVerificationHeader[] =
      "Sec-Attribution-Reporting-Private-State-Token";

  AttributionVerificationMediator(
      const TrustTokenKeyCommitmentGetter* key_commitment_getter,
      std::vector<std::unique_ptr<Cryptographer>> cryptographers,
      std::unique_ptr<MetricsRecorder> metrics_recorder);
  ~AttributionVerificationMediator();

  AttributionVerificationMediator(const AttributionVerificationMediator&) =
      delete;
  AttributionVerificationMediator& operator=(
      const AttributionVerificationMediator&) = delete;

  // Returns headers used for report verification if the `url`'s origin is
  // configured as a private state tokens issuer.
  //
  // 1. Get the issuer's key commitment; if unavailable or unsuccessful, returns
  //    no headers
  // 2. Blind the messages received; if unsuccessful, returns no headers
  // 3. Returns two headers;
  //    * Sec-Attribution-Reporting-Private-State-Token: with blinded messages
  //    serialized as a structured header list of string:
  //    https://www.rfc-editor.org/rfc/rfc8941.html#name-lists
  //    * Sec-Private-State-Token-Crypto-Version: with the protocol version
  //      configured in the issuers' key commitments
  //
  // `messages` is a vector of strings representing the data that we want to
  // attest to. Each message in `messages` will be blinded before being sent to
  // the issuer.
  //
  // Later, when receiving the data with a token, the issuer will need to
  // re-generate the message based on the data received to verify the token.
  void GetHeadersForVerification(
      const GURL& url,
      std::vector<Message>,
      base::OnceCallback<void(net::HttpRequestHeaders)> done);

  // Process headers from a verification response; if present and valid,
  // generates and returns tokens that can be used for redemption.
  // 1. Checks `response_headers` for a verification response header.
  // 2. If the header is present, strips it from `response_headers`, parses it
  //    as a structured header list of strings to obtain blind tokens. The
  //    number of blind tokens received can be smaller than the number of blind
  //    messages sent for signature. However, the order in which they are
  //    received must match the order in which they were sent.
  // 3. Each blind token is sent to an underlying cryptographic library which
  //    parses, validates and unblind the token. If any blind-token fails
  //    redemption, no tokens are returned.
  //
  // If all three steps are successful, returns tokens that can be sent to and
  // verified by the issuer that signed the blind messages. Otherwise, returns
  // an empty array.
  void ProcessVerificationToGetTokens(
      net::HttpResponseHeaders& response_headers,
      base::OnceCallback<void(std::vector<Token>)> done);

 private:
  struct CryptographersAndTokens;
  struct CryptographersAndBlindedMessages;

  static CryptographersAndBlindedMessages BeginIssuances(
      std::vector<std::unique_ptr<Cryptographer>>,
      const std::vector<Message>&);

  static CryptographersAndTokens ConfirmIssuancesAndBeginRedemptions(
      std::vector<std::unique_ptr<Cryptographer>>,
      std::vector<BlindedToken>);

  static std::string SerializeBlindedMessages(
      const std::vector<BlindedMessage>&);

  static std::vector<BlindedMessage> DeserializeBlindedTokens(
      const std::string& blind_tokens_header);

  // Continuation of `GetHeadersForVerification` after asynchronous key
  // commitment fetching concludes. `done` is `GetHeadersForVerification`'s
  // parameter, passed on to the continuation; `commitment_result` is the result
  // of the key commitment fetch.
  void OnGotKeyCommitment(
      base::OnceCallback<void(net::HttpRequestHeaders)> done,
      mojom::TrustTokenKeyCommitmentResultPtr commitment_result);

  // Continuation of `GetHeadersForVerification` after an off-thread execution
  // of issuance operation (`Cryptographer::BeginIssuance`).`protocol_version`
  // is the private state issuer configured protocol versions; `done` is
  // `GetHeadersForVerification`'s parameter, passed on to the continuation.
  // Receives ownership of the cryptographer back from the asynchronous callback
  // and stores it back in `cryptographers_` to reuse during
  // `ProcessVerificationToGetTokens`.
  void OnDoneBeginIssuance(
      mojom::TrustTokenProtocolVersion protocol_version,
      base::OnceCallback<void(net::HttpRequestHeaders)> done,
      CryptographersAndBlindedMessages);

  // Continuation of `ProcessVerificationToGetTokens` after an off-thread
  // execution to complete the issuance
  // (`Cryptographer::ConfirmIssuanceAndBeginRedemption`). `done` is
  // `ProcessVerificationToGetTokens`'s parameter, passed on to the
  // continuation. Receives ownership of the cryptographer back from the
  // asynchronous callback.
  void OnDoneProcessingIssuanceResponse(
      base::OnceCallback<void(std::vector<Token>)> done,
      CryptographersAndTokens);

  // `messages_` will be empty until it gets initialized in
  // `GetHeadersForVerification`. Once filled, it will never be mutated over the
  // course of the operation's execution.
  std::vector<Message> messages_;

  // The key_commitment_getter_ instance is a singleton owned by NetworkService,
  // it will always outlive this.
  const raw_ptr<const TrustTokenKeyCommitmentGetter, DanglingUntriaged>
      key_commitment_getter_;

  // Relinquishes ownership during posted tasks for the potentially
  // computationally intensive cryptographic operations
  // (Cryptographer::BeginIssuance,
  // Cryptographer::ConfirmIssuanceAndBeginRedemption); repopulated when
  // regaining ownership upon receiving each operation's results.
  //
  // A Cryptographer does not support concurrent issuance requests. When
  // `GetHeadersForVerification` is called with multiple messages, we need to
  // start an issuance per message. As a result, the number of cryptographers
  // must match the number of messages received.
  // TODO(https://crbug.com/1440838): use batch issuance instead of N
  // cryptographers when BorringSSL adds support for it.
  std::vector<std::unique_ptr<Cryptographer>> cryptographers_;

  // The metrics recorder will be defined for the full lifecycle of this.
  std::unique_ptr<MetricsRecorder> metrics_recorder_;

  base::WeakPtrFactory<AttributionVerificationMediator> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_VERIFICATION_MEDIATOR_H_
