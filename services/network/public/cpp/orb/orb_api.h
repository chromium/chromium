// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ORB_ORB_API_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ORB_ORB_API_H_

#include <optional>
#include <set>
#include <string_view>

#include "base/component_export.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

// This header file provides the public API that supports applying ORB to HTTP
// responses.  For more details and resources about ORB please see the
// README.md file.

namespace network::orb {

// Used to strip response headers if CORB made a decision to block the response.
COMPONENT_EXPORT(NETWORK_SERVICE)
void SanitizeBlockedResponseHeaders(network::mojom::URLResponseHead& response);

// Per-URLLoaderFactory state (used by ORB for marking specific URLs as media
// and allowing them in subsequent range requests;  constructed and passed both
// for CORB and ORB for consistency and ease of implementation).
using PerFactoryState = std::set<GURL>;

// ResponseAnalyzer is a pure, virtual interface that can be implemented by
// either CORB or ORB.
class COMPONENT_EXPORT(NETWORK_SERVICE) ResponseAnalyzer {
 public:
  // Creates a ResponseAnalyzer.
  //
  // The caller needs to guarantee that `state`'s lifetime is at least as long
  // as the lifetime of `ResponseAnalyzer`.  `state` needs to be non-null.
  static std::unique_ptr<ResponseAnalyzer> Create(PerFactoryState* state);

  // Decision for what to do with the HTTP response being analyzed.
  enum class Decision {
    kAllow,
    kBlock,
    kSniffMore,
  };

  // Decision for how to signal a blocking decision to the network stack and
  // the application.
  enum class BlockedResponseHandling {
    kEmptyResponse,
    kNetworkError,
  };

  // The Init method should be called exactly once after getting the
  // ResponseAnalyzer from the Create method.  The Init method attempts to
  // calculate the `Decision` based on the HTTP response headers.  If
  // `kSniffMore` is returned, then Sniff or HandleEndOfSniffableResponseBody
  // needs to be called to reach the `Decision`.
  //
  // Implementations of this method can assume that callers pass a trustworthy
  // |request_initiator| (e.g. one that can't be spoofed by a compromised
  // renderer). This is generally true for
  // network::ResourceRequest::request_initiator within NetworkService (see the
  // enforcement in CorsURLLoaderFactory::IsValidRequest).
  virtual Decision Init(
      const GURL& request_url,
      const std::optional<url::Origin>& request_initiator,
      mojom::RequestMode request_mode,
      mojom::RequestDestination request_destination_from_renderer,
      const network::mojom::URLResponseHead& response) = 0;

  // The Sniff method should be called if an earlier call to Init (or Sniff)
  // returned Decision::kSniffMore.  This method will attempt to calculate the
  // `Decision` based on the (prefix of the) HTTP response body.
  virtual Decision Sniff(std::string_view response_body) = 0;

  // The HandleEndOfSniffableResponseBody should be called if earlier calls to
  // Init/Sniff returned kSniffMore, but there is nothing more to sniff (because
  // the end of the response body was reached, or because we've reached the
  // maximum number of bytes to sniff).  This method will return kAllow or
  // kBlock (and should not return kSniffMore).
  virtual Decision HandleEndOfSniffableResponseBody() = 0;

  // True if the analyzed response should report the blocking decision in a
  // warning message written to the DevTools console.
  virtual bool ShouldReportBlockedResponse() const = 0;

  // How should a blocked response be treated?
  virtual BlockedResponseHandling ShouldHandleBlockedResponseAs() const = 0;

  virtual ~ResponseAnalyzer();
};

}  // namespace network::orb

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ORB_ORB_API_H_
