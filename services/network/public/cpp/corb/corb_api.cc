// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/corb/corb_api.h"

#include <string>
#include <unordered_set>

#include "base/metrics/histogram_functions.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/corb/corb_impl.h"
#include "services/network/public/cpp/corb/orb_impl.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network::corb {

namespace {

void RemoveAllHttpResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  DCHECK(headers);
  std::unordered_set<std::string> names_of_headers_to_remove;

  size_t it = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&it, &name, &value))
    names_of_headers_to_remove.insert(base::ToLowerASCII(name));

  headers->RemoveHeaders(names_of_headers_to_remove);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Comparison {
  kInvalid = 0,
  kBothAgreeInFinalDecision = 1,
  kCorbBlocksAndOrbDoesnt = 2,
  kOrbBlocksAndCorbDoesnt = 3,
  kBothWantToSniffMore = 4,
  kCorbWantsToSniffMore = 5,
  kOrbWantsToSniffMore = 6,
  kMaxValue = kOrbWantsToSniffMore,  // For UMA histograms.
};

// TODO(https://crbug.com/1178928): Stop running *both* CORB and ORB together,
// once we've gathered enough UMA data.
class ComparingAnalyzer : public ResponseAnalyzer {
 public:
  explicit ComparingAnalyzer(PerFactoryState& state)
      : corb_analyzer_(
            std::make_unique<CrossOriginReadBlocking::CorbResponseAnalyzer>()),
        orb_analyzer_(std::make_unique<OpaqueResponseBlockingAnalyzer>(state)),
        is_orb_enabled_(base::FeatureList::IsEnabled(
            features::kOpaqueResponseBlockingV01)) {}

  ~ComparingAnalyzer() override {
    Comparison comparison = Comparison::kInvalid;
    if (corb_decision_ != Decision::kSniffMore &&
        orb_decision_ != Decision::kSniffMore) {
      if (corb_decision_ == orb_decision_) {
        comparison = Comparison::kBothAgreeInFinalDecision;
      } else if (corb_decision_ == Decision::kBlock) {
        DCHECK_EQ(Decision::kAllow, orb_decision_);
        comparison = Comparison::kCorbBlocksAndOrbDoesnt;
      } else {
        DCHECK_EQ(Decision::kAllow, corb_decision_);
        DCHECK_EQ(Decision::kBlock, orb_decision_);
        comparison = Comparison::kOrbBlocksAndCorbDoesnt;

        orb_analyzer_->ReportOrbBlockedAndCorbDidnt();
      }
    } else {
      if (corb_decision_ == orb_decision_) {
        DCHECK_EQ(Decision::kSniffMore, corb_decision_);
        DCHECK_EQ(Decision::kSniffMore, orb_decision_);
        comparison = Comparison::kBothWantToSniffMore;
      } else if (corb_decision_ == Decision::kSniffMore) {
        comparison = Comparison::kCorbWantsToSniffMore;
      } else {
        DCHECK_EQ(Decision::kSniffMore, orb_decision_);
        comparison = Comparison::kOrbWantsToSniffMore;
      }
    }

    base::UmaHistogramEnumeration("SiteIsolation.ORB.CorbVsOrb", comparison);
  }

  ComparingAnalyzer(const ComparingAnalyzer&) = delete;
  ComparingAnalyzer& operator=(const ComparingAnalyzer&) = delete;

  Decision Init(const GURL& request_url,
                const absl::optional<url::Origin>& request_initiator,
                mojom::RequestMode request_mode,
                const network::mojom::URLResponseHead& response) override {
    corb_decision_ = corb_analyzer_->Init(request_url, request_initiator,
                                          request_mode, response);
    orb_decision_ = orb_analyzer_->Init(request_url, request_initiator,
                                        request_mode, response);
    return GetCombinedDecision();
  }

  Decision Sniff(base::StringPiece response_body) override {
    if (corb_decision_ == Decision::kSniffMore)
      corb_decision_ = corb_analyzer_->Sniff(response_body);
    if (orb_decision_ == Decision::kSniffMore)
      orb_decision_ = orb_analyzer_->Sniff(response_body);
    return GetCombinedDecision();
  }

  Decision HandleEndOfSniffableResponseBody() override {
    if (corb_decision_ == Decision::kSniffMore)
      corb_decision_ = corb_analyzer_->HandleEndOfSniffableResponseBody();
    if (orb_decision_ == Decision::kSniffMore)
      orb_decision_ = orb_analyzer_->HandleEndOfSniffableResponseBody();
    return GetCombinedDecision();
  }

  bool ShouldReportBlockedResponse() const override {
    return GetEnabledAnalyzer().ShouldReportBlockedResponse();
  }

  BlockedResponseHandling ShouldHandleBlockedResponseAs() const override {
    return GetEnabledAnalyzer().ShouldHandleBlockedResponseAs();
  }

 private:
  Decision GetCombinedDecision() {
    if ((corb_decision_ == Decision::kSniffMore) ||
        (orb_decision_ == Decision::kSniffMore)) {
      // At least one of the analyzers didn't reach the final decision yet.
      return Decision::kSniffMore;
    }

    return is_orb_enabled_ ? orb_decision_ : corb_decision_;
  }

  const ResponseAnalyzer& GetEnabledAnalyzer() const {
    if (is_orb_enabled_)
      return *orb_analyzer_;
    else
      return *corb_analyzer_;
  }

  const std::unique_ptr<CrossOriginReadBlocking::CorbResponseAnalyzer>
      corb_analyzer_;
  const std::unique_ptr<OpaqueResponseBlockingAnalyzer> orb_analyzer_;
  const bool is_orb_enabled_ = false;

  Decision corb_decision_ = Decision::kSniffMore;
  Decision orb_decision_ = Decision::kSniffMore;
};

}  // namespace

ResponseAnalyzer::~ResponseAnalyzer() = default;

// static
std::unique_ptr<ResponseAnalyzer> ResponseAnalyzer::Create(
    PerFactoryState& state) {
  return std::make_unique<ComparingAnalyzer>(state);
}

void SanitizeBlockedResponseHeaders(network::mojom::URLResponseHead& response) {
  response.content_length = 0;
  if (response.headers)
    RemoveAllHttpResponseHeaders(response.headers);
}

}  // namespace network::corb
