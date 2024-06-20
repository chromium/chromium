// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORB_ORB_IMPL_H_
#define SERVICES_NETWORK_ORB_ORB_IMPL_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

// This header provides an implementation of Opaque Response Blocking (ORB).
// The implementation should typically be used through the public orb_api.h.
// See the doc comment in orb_api.h for a more detailed description of ORB.

namespace network::orb {

class COMPONENT_EXPORT(NETWORK_SERVICE) OpaqueResponseBlockingAnalyzer final
    : public ResponseAnalyzer {
 public:
  // The caller needs to guarantee that `state`'s lifetime is at least as long
  // as the lifetime of `OpaqueResponseBlockingAnalyzer`.  `state` needs to be
  // non-null.
  explicit OpaqueResponseBlockingAnalyzer(PerFactoryState* state);

  OpaqueResponseBlockingAnalyzer(const OpaqueResponseBlockingAnalyzer&) =
      delete;
  OpaqueResponseBlockingAnalyzer& operator=(
      const OpaqueResponseBlockingAnalyzer&) = delete;

  // Implementation of the `orb::ResponseAnalyzer` abstract interface.
  ~OpaqueResponseBlockingAnalyzer() override;
  Decision Init(const GURL& request_url,
                const std::optional<url::Origin>& request_initiator,
                mojom::RequestMode request_mode,
                mojom::RequestDestination request_destination_from_renderer,
                const network::mojom::URLResponseHead& response) override;
  Decision Sniff(std::string_view data) override;
  Decision HandleEndOfSniffableResponseBody() override;
  bool ShouldReportBlockedResponse() const override;
  BlockedResponseHandling ShouldHandleBlockedResponseAs() const override;

  // Each `return Decision::kBlock` in the implementation of
  // OpaqueResponseBlockingAnalyzer has a corresponding enum value below.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class BlockingDecisionReason {
    kInvalid = 0,
    kNeverSniffedMimeType = 1,
    kNoSniffHeader = 2,
    kUnexpectedRangeResponse = 3,
    kSniffedAsHtml = 4,
    kSniffedAsXml = 5,
    kSniffedAsJson = 6,
    kMaxValue = kSniffedAsJson,  // For UMA histograms.
  };

 private:
  void StoreAllowedAudioVideoRequest(const GURL& media_url);
  bool IsAllowedAudioVideoRequest(const GURL& media_url);

  // The MIME type from the `Content-Type` header of the HTTP response.
  std::string mime_type_;

  // Whether the HTTP status is OK according to
  // https://fetch.spec.whatwg.org/#ok-status.
  bool is_http_status_okay_ = true;

  // Whether the `X-Content-Type-Options: nosniff` HTTP response header is
  // present.
  bool is_no_sniff_header_present_ = true;

  // Final request URL (final = after all redirects).
  GURL final_request_url_;

  // Whether Content length was 0, or the HTTP status was 204.
  bool is_empty_response_ = false;

  // Whether the response carried Attribution Reporting headers.
  bool is_attribution_response_ = false;

  // Remembering which past requests sniffed as media.  Never null.
  raw_ref<PerFactoryState> per_factory_state_;

  BlockingDecisionReason blocking_decision_reason_ =
      BlockingDecisionReason::kInvalid;

  // The request destination. Note that this value always originates from
  // the renderer.
  mojom::RequestDestination request_destination_from_renderer_ =
      mojom::RequestDestination::kEmpty;
};

}  // namespace network::orb

#endif  // SERVICES_NETWORK_ORB_ORB_IMPL_H_
