// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_
#define SERVICES_NETWORK_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/types.h"
#include "url/gurl.h"
namespace network {
namespace test {

struct TrustTokenSignedRequest {
  GURL destination;
  net::HttpRequestHeaders headers;
};

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

  enum class ServerOperationOutcome {
    kExecuteOperationAsNormal,
    kUnconditionalFailure,
  };

  struct Options final {
    Options();
    ~Options();
    Options(const Options&);
    Options& operator=(const Options&);

    // The number of issuance key pairs to provide via key commitment results.
    int num_keys = 1;

    // The protocol version with which to parameterize the server-side
    // cryptographic logic. We return this value in key commitment results.
    std::string protocol_version = internal::ProtocolVersionToString(
        mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb);

    // The key commitment ID.
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

    // The following two fields specify operating systems on which to specify
    // that the browser should attempt platform-provided trust token issuance
    // instead of sending requests directly to the issuer's server, and the
    // fallback behavior when these operations are unavailable. This information
    // will be included in GetKeyCommitmentRecord's returned commitments.
    std::set<mojom::TrustTokenKeyCommitmentResult::Os>
        specify_platform_issuance_on;
    mojom::TrustTokenKeyCommitmentResult::UnavailableLocalOperationFallback
        unavailable_local_operation_fallback =
            mojom::TrustTokenKeyCommitmentResult::
                UnavailableLocalOperationFallback::kReturnWithError;
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
  std::optional<std::string> Issue(std::string_view issuance_request);

  // Given a base64-encoded redemption request, processes the
  // request and returns either nullopt (on error) or a string containing
  // the metadata values.
  std::optional<std::string> Redeem(std::string_view redemption_request);

  // Stores a representation of a signed request with the given destination and
  // headers in a manner that can be retrieved for inspection by calling
  // |last_incoming_signed_request|.
  void RecordSignedRequest(const GURL& destination,
                           const net::HttpRequestHeaders& headers);

  // Returns the public key hashes received in prior redemption requests.
  std::set<std::string> hashes_of_redemption_bound_public_keys() const;

  // Returns a structured representation of the last signed request received.
  std::optional<TrustTokenSignedRequest> last_incoming_signed_request() const;

 private:
  struct Rep;  // Contains state internal to this class's implementation.

  // Guards this class's internal state. This makes sure we're reading writes to
  // the state that occur while handling requests, which takes place off of the
  // main sequence due to how net::EmbeddedTestServer works.
  mutable base::Lock mutex_;
  std::unique_ptr<Rep> rep_ GUARDED_BY(mutex_);
};

}  // namespace test
}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TRUST_TOKEN_REQUEST_HANDLER_H_
