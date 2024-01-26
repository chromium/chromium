// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_CONTROLLER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_CONTROLLER_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
struct RedirectInfo;
class URLRequest;
}  // namespace net

namespace url {
class Origin;
}

namespace network {

namespace mojom {
class URLLoaderFactory;
class URLResponseHead;
}  // namespace mojom

namespace internal {

// Creates a key commitment request for the given issuance
// or redemption request:
// 1. sets the LOAD_BYPASS_CACHE and LOAD_DISABLE_CACHE flags,
// so that the result doesn't check the cache and isn't cached itself
// 2. sets the URL to kTrustTokenKeyCommitmentWellKnownPath, resolved
// relative to the issuance or redemption origin
// 3. sets the key commitment request to be uncredentialed
// 4. copies |request|'s initiator to the key commitment request
// 5. sets the key commitment request's Origin header to equal |request|'s
// top-level origin. (This is so servers can make a decision about whether to
// reject issuance or redemption early, by making a general decision about
// whether they want to issue/redeem on the provided top-level origin.)
std::unique_ptr<ResourceRequest> CreateTrustTokenKeyCommitmentRequest(
    const net::URLRequest& request,
    const url::Origin& top_level_origin);

}  // namespace internal

// TrustTokenKeyCommitmentController executes a single Trust Tokens key
// commitment request.
//
// This is an uncredentialed request to the above .well-known path
// relative to the origin of the Trust Tokens issuer involved in an issuance or
// redemption's origin; the request expects a key commitment response of the
// format defined in the Privacy Pass draft spec:
// https://github.com/alxdavids/draft-privacy-pass/blob/master/draft-privacy-pass.md.
//
// Lifetime: These are expected to be constructed when the client
// wishes to execute a request and destroyed immediately after the client
// receives its result.
class TrustTokenKeyCommitmentController final {
 public:
  // Class Parser parses HTTP response bodies obtained from key commitment
  // registry queries.
  class Parser {
   public:
    virtual ~Parser() = default;
    virtual mojom::TrustTokenKeyCommitmentResultPtr Parse(
        std::string_view response_body) = 0;
  };

  // Constructor. Immediately starts a request:
  // 1. builds a key commitment request using metadata from |request| (along
  // with |top_level_origin|, which must be |request|'s initiating top level
  // frame's origin);
  // 2. uses |loader_factory| to send the key commitment request to
  // |kTrustTokenKeyCommitmentWellKnownPath|, resolved relative to |request|'s
  // origin;
  // 3. uses |parser| to parse the result;
  // 4. on completion or error, calls |completion_callback| with an error code
  // and, if successful, a result.
  struct Status {
    enum class Value {
      // There was an error parsing the key commitment endpoint's response. In
      // particular, this occurs if servers deliberately return an empty or
      // malformed response to short-circuit issuance or redemption.
      kCouldntParse,
      // The key commitment endpoint responded with a redirect, which is not
      // permitted.
      kGotRedirected,
      // Success.
      kOk,
      // Connection error (|net_error| contains the specific error code).
      kNetworkError,
    } value;
    int net_error;
  };
  TrustTokenKeyCommitmentController(
      base::OnceCallback<void(Status status,
                              mojom::TrustTokenKeyCommitmentResultPtr result)>
          completion_callback,
      const net::URLRequest& request,
      const url::Origin& top_level_origin,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      mojom::URLLoaderFactory* loader_factory,
      std::unique_ptr<Parser> parser);

  TrustTokenKeyCommitmentController(const TrustTokenKeyCommitmentController&) =
      delete;
  TrustTokenKeyCommitmentController& operator=(
      const TrustTokenKeyCommitmentController&) = delete;

  ~TrustTokenKeyCommitmentController();

 private:
  void StartRequest(mojom::URLLoaderFactory* loader_factory);

  // Callbacks provided to |url_loader_|:

  // On redirect, fails (key commitment endpoints must not redirect
  // their clients).
  void HandleRedirect(const GURL& url_before_redirect,
                      const net::RedirectInfo& redirect_info,
                      const mojom::URLResponseHead& response_head,
                      std::vector<std::string>* to_be_removed_headers);

  // On completion, parses the given response (if the request was
  // successful). Calls |completion_callback_| with an error
  void HandleResponseBody(std::unique_ptr<std::string> response_body);

  // |url_loader_| performs the actual key commitment request.
  std::unique_ptr<SimpleURLLoader> url_loader_;

  // Parses the key commitment response if one is received.
  std::unique_ptr<Parser> parser_;

  base::OnceCallback<void(Status status,
                          mojom::TrustTokenKeyCommitmentResultPtr result)>
      completion_callback_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_CONTROLLER_H_
