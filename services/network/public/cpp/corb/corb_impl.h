// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORB_CORB_IMPL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORB_CORB_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "services/network/public/cpp/corb/corb_api.h"
#include "services/network/public/cpp/corb/orb_mimetypes.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

// This header provides an implementation of Cross-Origin Read Blocking (CORB).
// The implementation should typically be used through the public corb_api.h.
// See the doc comment in corb_api.h for a more detailed description of CORB.

namespace content {
FORWARD_DECLARE_TEST(CrossSiteDocumentResourceHandlerTest, ResponseBlocking);
FORWARD_DECLARE_TEST(CrossSiteDocumentResourceHandlerTest,
                     CORBProtectionLogging);
}  // namespace content

namespace network {
namespace corb {

class COMPONENT_EXPORT(NETWORK_CPP) CrossOriginReadBlocking {
 public:
  // Not instantiable - only static methods.
  CrossOriginReadBlocking() = delete;

  // An instance for tracking the state of analyzing a single response
  // and deciding whether CORB should block the response.
  class COMPONENT_EXPORT(NETWORK_CPP) CorbResponseAnalyzer final
      : public network::corb::ResponseAnalyzer {
   public:
    // Categorizes the resource MIME type for CORB protection logging.
    enum MimeTypeBucket {
      // MIME types we expect CORB to protect (HTML, JSON etc.).
      kProtected = 0,
      // MIME types we expect CORB to allow through (js, images).
      kPublic,
      // Other MIME types.
      kOther,
    };
    // Enum for CORB protection logging. Records if CORB would block a request
    // if it was made cross-origin. This enum backs a histogram, so do not
    // change the order of entries or remove entries. When adding new entries
    // update |kMaxValue| and enums.xml (see the CrossOriginProtectionDecision
    // enum).
    enum class CrossOriginProtectionDecision {
      // Logged if the request would be allowed without sniffing.
      kAllow = 0,
      // Logged if the request would be blocked without sniffing.
      kBlock = 1,
      // Logged if the request needed more sniffing for a decision.
      kNeedToSniffMore = 2,
      // Logged if the request would be allowed after sniffing.
      kAllowedAfterSniffing = 3,
      // Logged if the request would be blocked after sniffing.
      kBlockedAfterSniffing = 4,

      kMaxValue = kBlockedAfterSniffing,
    };

    CorbResponseAnalyzer();

    CorbResponseAnalyzer(const CorbResponseAnalyzer&) = delete;
    CorbResponseAnalyzer& operator=(const CorbResponseAnalyzer&) = delete;

    // Implementation of the `corb::ResponseAnalyzer` abstract interface.
    ~CorbResponseAnalyzer() override;
    Decision Init(
        const GURL& request_url,
        const std::optional<url::Origin>& request_initiator,
        mojom::RequestMode request_mode,
        mojom::RequestDestination /*request_destination_from_renderer*/,
        const network::mojom::URLResponseHead& response) override;
    Decision Sniff(std::string_view data) override;
    Decision HandleEndOfSniffableResponseBody() override;
    bool ShouldReportBlockedResponse() const override;
    BlockedResponseHandling ShouldHandleBlockedResponseAs() const override;

    class ConfirmationSniffer;
    class SimpleConfirmationSniffer;

    // Returns true if the response has a nosniff header.
    static bool HasNoSniff(const network::mojom::URLResponseHead& response);

   private:
    FRIEND_TEST_ALL_PREFIXES(CrossOriginReadBlockingTest,
                             SeemsSensitiveFromCORSHeuristic);
    FRIEND_TEST_ALL_PREFIXES(CrossOriginReadBlockingTest,
                             SeemsSensitiveFromCacheHeuristic);
    FRIEND_TEST_ALL_PREFIXES(CrossOriginReadBlockingTest,
                             SeemsSensitiveWithBothHeuristics);
    FRIEND_TEST_ALL_PREFIXES(CrossOriginReadBlockingTest,
                             SupportsRangeRequests);
    FRIEND_TEST_ALL_PREFIXES(content::CrossSiteDocumentResourceHandlerTest,
                             CORBProtectionLogging);
    FRIEND_TEST_ALL_PREFIXES(ResponseAnalyzerTest, CORBProtectionLogging);

    // true if either 1) ShouldBlockBasedOnHeaders decided to allow the response
    // based on headers alone or 2) ShouldBlockBasedOnHeaders decided to sniff
    // the response body and SniffResponseBody decided to allow the response
    // (e.g. because none of sniffers found blockable content).  false
    // otherwise.
    bool ShouldAllow() const;

    // true if either 1) ShouldBlockBasedOnHeaders decided to block the response
    // based on headers alone or 2) ShouldBlockBasedOnHeaders decided to sniff
    // the response body and SniffResponseBody confirmed that the response
    // contains blockable content.  false otherwise.
    bool ShouldBlock() const;

    // true if the analyzed response should report Cross-Origin Read Blocking in
    // a warning message written to the DevTools console.

    // Whether ShouldBlockBasedOnHeaders asked to sniff the body or the CORB
    // protection logging needs extra sniffing.
    bool needs_sniffing() const {
      return should_block_based_on_headers_ == Decision::kSniffMore ||
             corb_protection_logging_needs_sniffing_;
    }

    // Helper for translating ShouldAllow(), ShouldBlock() and needs_sniffing()
    // into corb::Decision.
    Decision GetCorbDecision();

    // Static because this method is called both during the actual decision, and
    // for the CORB protection logging decision.
    static Decision ShouldBlockBasedOnHeaders(
        mojom::RequestMode request_mode,
        const GURL& request_url,
        const std::optional<url::Origin>& request_initiator,
        const network::mojom::URLResponseHead& response,
        MimeType canonical_mime_type);

    // Checks if the response seems sensitive for CORB protection logging.
    // Returns true if the Access-Control-Allow-Origin header has a value other
    // than *.
    static bool SeemsSensitiveFromCORSHeuristic(
        const network::mojom::URLResponseHead& response);

    // Checks if the response seems sensitive for CORB protection logging.
    // Returns true if the response has Vary: Origin and Cache-Control: Private
    // headers.
    static bool SeemsSensitiveFromCacheHeuristic(
        const network::mojom::URLResponseHead& response);

    // Checks if a response has an Accept-Ranges header. This indicates the
    // server supports range requests which may allow bypassing CORB due to
    // their multipart content type.
    static bool SupportsRangeRequests(
        const network::mojom::URLResponseHead& response_headers);

    // Determines the MIME type bucket for CORB protection logging.
    static MimeTypeBucket GetMimeTypeBucket(
        const network::mojom::URLResponseHead& response);

    // Returns a protection decision (blocked after sniffing or allowed after
    // sniffing) depending on if the sniffers found blockable content.
    static CrossOriginProtectionDecision SniffingDecisionToProtectionDecision(
        bool found_blockable_content);

    // Populates |sniffers_| container based on |canonical_mime_type_|.  Called
    // if ShouldBlockBasedOnHeaders returns kSniffMore
    void CreateSniffers();

    // Outcome of ShouldBlockBasedOnHeaders recorded inside the Create method.
    Decision should_block_based_on_headers_ = Decision::kBlock;

    // The following values store information about the response.
    bool corb_protection_logging_needs_sniffing_ = false;
    // |mime_type_bucket_| is either kProtected (if it's a type we expect to
    // protect such as HTML), kPublic (for javascript etc.) or kOther.
    MimeTypeBucket mime_type_bucket_ = kOther;

    // A few booleans that are computed based on the response headers.
    bool seems_sensitive_from_cors_heuristic_ = false;
    bool seems_sensitive_from_cache_heuristic_ = false;
    bool supports_range_requests_ = false;
    bool has_nosniff_header_ = false;

    // |hypothetical_sniffing_mode_| is true if we need to sniff only because of
    // the CORB protection logging (and otherwise, CORB would not sniff).
    bool hypothetical_sniffing_mode_ = false;

    // Canonical MIME type detected by ShouldBlockBasedOnHeaders.  Used to
    // determine if blocking the response is needed, as well as which type of
    // sniffing to perform.
    MimeType canonical_mime_type_ = MimeType::kInvalidMimeType;

    // Content length if available. -1 if not available.
    int64_t content_length_ = -1;

    // The HTTP response code (e.g. 200 or 404) received in response to this
    // resource request.
    int http_response_code_ = 0;

    // The sniffers to be used.
    std::vector<std::unique_ptr<ConfirmationSniffer>> sniffers_;

    // Sniffing results.
    bool found_blockable_content_ = false;
  };

  // This enum backs a histogram, so do not change the order of entries or
  // remove entries. When adding new entries update |kMaxValue| and enums.xml
  // (see the SiteIsolationResponseAction enum).
  enum class Action {
    // Logged at OnResponseStarted.
    kResponseStarted = 0,

    // Logged when a response is blocked without requiring sniffing.
    kBlockedWithoutSniffing = 1,

    // Logged when a response is blocked as a result of sniffing the content.
    kBlockedAfterSniffing = 2,

    // Logged when a response is allowed without requiring sniffing.
    kAllowedWithoutSniffing = 3,

    // Logged when a response is allowed as a result of sniffing the content.
    kAllowedAfterSniffing = 4,

    kMaxValue = kAllowedAfterSniffing
  };
};

}  // namespace corb
}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORB_CORB_IMPL_H_
