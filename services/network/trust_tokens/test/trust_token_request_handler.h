// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_

#include <string>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace network {
namespace test {

// TrustTokenRequestHandler encapsulates server-side Trust Tokens issuance and
// redemption logic and implements some integrity and correctness checks for
// requests subsequently signed with keys bound to token redemptions.
//
// It's thread-safe so that the methods can be called by test code directly and
// by net::EmbeddedTestServer handlers.
class TrustTokenRequestHandler {
 public:
  struct Options;  // Definition below.
  explicit TrustTokenRequestHandler(Options options);

  // The default constructor uses reasonable default options.
  TrustTokenRequestHandler();

  ~TrustTokenRequestHandler();

  // TODO(davidvc): Provide a way to specify when keys expire.

  // See |Options::client_signing_outcome| below.
  enum class SigningOutcome {
    // Expect a well-formed SRR and possibly a Sec-Signature header.
    kSuccess,
    // Expect an empty Sec-Signed-Redemption-Record header and no Sec-Signature
    // header.
    kFailure,
  };

  enum class ServerOperationOutcome {
    kExecuteOperationAsNormal,
    kUnconditionalFailure,
  };

  struct Options {
    // The number of issuance key pairs to provide via key commitment results.
    int num_keys = 1;

    // Specifies whether the client-side signing operation is expected to
    // succeed. Unlike issuance and redemption, clients send signed requests
    // even when the operation failures, but the outcome affects the shape of
    // the expected request.
    SigningOutcome client_signing_outcome = SigningOutcome::kSuccess;

    // The protocol version to use.
    std::string protocol_version = "TrustTokenV1";

    // The commitment ID to use.
    int id = 1;

    // The number of tokens to sign per issuance operation; this value is also
    // provided to the client as part of key commitment results.
    int batch_size = 10;

    // If set to |kUnconditionalFailure|, returns a failure response for the
    // corresponding operation even if the operation would have succeeded had
    // the server been operating correctly.
    ServerOperationOutcome issuance_outcome =
        ServerOperationOutcome::kExecuteOperationAsNormal;
    ServerOperationOutcome redemption_outcome =
        ServerOperationOutcome::kExecuteOperationAsNormal;
  };

  // Updates the handler's options, resetting its internal state.
  void UpdateOptions(Options options);

  // Returns a key commitment record suitable for inserting into a {issuer:
  // commitment} dictionary passed to the network service via
  // NetworkService::SetTrustTokenKeyCommitments. This comprises |num_keys|
  // token verification keys, a protocol version of |protocol_version|, an ID of
  // |id| and  a batch size of |batch_size| (or none if |batch_size| is
  // nullopt).
  std::string GetKeyCommitmentRecord() const;

  // Given a base64-encoded issuance request, processes the
  // request and returns either nullopt (on error) or a base64-encoded response.
  base::Optional<std::string> Issue(base::StringPiece issuance_request);

  // Given a base64-encoded redemption request, processes the
  // request and returns either nullopt (on error) or a base64-encoded response.
  // On success, the response's signed redemption record will have a lifetime of
  // |kSrrLifetime|. We use a ludicrously long lifetime because there's no way
  // to mock time in browser tests, and we don't want the SRR expiring
  // unexpectedly.
  //
  // TODO(davidvc): This needs to be expanded to be able to provide
  // SRRs that have already expired. (This seems like the easiest way of
  // exercising client-side SRR expiry logic in end-to-end tests, because
  // there's no way to fast-forward a clock past an expiry time.)
  static const base::TimeDelta kSrrLifetime;
  base::Optional<std::string> Redeem(base::StringPiece redemption_request);

  // Inspects |request| to see if its contents are the expected the result of a
  // client-side signing operation.
  //
  // If the configured signing outcome (see Options) is kFailure, returns true
  // exactly when the request contains an empty Sec-Signed-Redemption-Record
  // header and no Sec-Signature header.
  //
  // If the configured signing outcome (see Options) is kSuccess, returns true
  // exactly when:
  // - the request bears a well-formed Sec-Signature header with a valid
  // signature over the request's canonical signing data; and
  // - the signature's public key's hash was bound to a previous redemption
  // request; and
  // - the request contains a well-formed signed redemption record whose
  // signature verifies against the issuer's published SRR key.
  //
  // Otherwise, returns false and, if |error_out| is not null, sets |error_out|
  // to a helpful error message.
  //
  // TODO(davidvc): This currently doesn't support signRequestData: 'omit'.
  bool VerifySignedRequest(const GURL& destination,
                           const net::HttpRequestHeaders& headers,
                           std::string* error_out = nullptr);

  // Returns the verification error from the most recent unsuccessful
  // VerifySignedRequest call, if any.
  base::Optional<std::string> LastVerificationError();

 private:
  struct Rep;  // Contains state internal to this class's implementation.

  // Guards this class's internal state.
  mutable base::Lock mutex_;
  std::unique_ptr<Rep> rep_ GUARDED_BY(mutex_);
};

}  // namespace test
}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_
