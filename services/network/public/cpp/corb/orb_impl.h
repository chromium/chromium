// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORB_ORB_IMPL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORB_ORB_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "services/network/public/cpp/corb/corb_api.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

// This header provides an implementation of Opaque Response Blocking (ORB).
// The implementation should typically be used through the public corb_api.h.
// See the doc comment in corb_api.h for a more detailed description of ORB.

namespace network {
namespace corb {

class COMPONENT_EXPORT(NETWORK_CPP) OpaqueResponseBlockingAnalyzer final
    : public ResponseAnalyzer {
 public:
  explicit OpaqueResponseBlockingAnalyzer(PerFactoryState& state);

  OpaqueResponseBlockingAnalyzer(const OpaqueResponseBlockingAnalyzer&) =
      delete;
  OpaqueResponseBlockingAnalyzer& operator=(
      const OpaqueResponseBlockingAnalyzer&) = delete;

  // Implementation of the `corb::ResponseAnalyzer` abstract interface.
  ~OpaqueResponseBlockingAnalyzer() override;
  Decision Init(const GURL& request_url,
                const absl::optional<url::Origin>& request_initiator,
                mojom::RequestMode request_mode,
                mojom::RequestDestination request_destination_from_renderer,
                const network::mojom::URLResponseHead& response) override;
  Decision Sniff(base::StringPiece data) override;
  Decision HandleEndOfSniffableResponseBody() override;
  bool ShouldReportBlockedResponse() const override;
  BlockedResponseHandling ShouldHandleBlockedResponseAs() const override;

  // TODO(https://crbug.com/1178928): Remove this once we gather enough
  // DumpWithoutCrashing data.
  void ReportOrbBlockedAndCorbDidnt() const;

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

  // Remembering which past requests sniffed as media.  Never null.
  // TODO(lukasza): Replace with raw_ref<T> or nonnull_raw_ptr<T> once
  // available.
  // This dangling raw_ptr occurred in:
  // content_browsertests: SignedExchangeSubresourcePrefetchBrowserTest.CORS
  // https://ci.chromium.org/ui/p/chromium/builders/try/mac-rel/1416613/test-results?q=ExactID%3Aninja%3A%2F%2Fcontent%2Ftest%3Acontent_browsertests%2FSignedExchangeSubresourcePrefetchBrowserTest.CORS+VHash%3Ae04c2114e5be4931
  raw_ptr<PerFactoryState, FlakyDanglingUntriaged> per_factory_state_;

  BlockingDecisionReason blocking_decision_reason_ =
      BlockingDecisionReason::kInvalid;

  // The request destination. Note that this value always originates from
  // the renderer.
  mojom::RequestDestination request_destination_from_renderer_ =
      mojom::RequestDestination::kEmpty;
};

}  // namespace corb
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORB_ORB_IMPL_H_
