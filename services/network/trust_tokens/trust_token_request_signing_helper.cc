// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "net/http/http_util.h"
#include "net/http/structured_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "url/url_constants.h"

namespace network {

namespace {

const char kRedemptionRecordHeaderRedemptionRecordKey[] = "redemption-record";

void LogOutcome(const net::NetLogWithSource& log, std::string_view outcome) {
  log.EndEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_SIGNING,
      [outcome]() { return base::Value::Dict().Set("outcome", outcome); });
}

}  // namespace

namespace {

using Params = TrustTokenRequestSigningHelper::Params;

void AttachRedemptionRecordHeader(net::HttpRequestHeaders& request_headers,
                                  std::string value) {
  request_headers.SetHeader(kTrustTokensRequestHeaderSecRedemptionRecord,
                            value);
}

// Builds a Trust Tokens redemption record header, which is logically an
// issuer-to-RR map but implemented as a Structured Headers Draft 15
// parameterized list (essentially a list where each member has an associated
// dictionary).
std::optional<std::string> ConstructRedemptionRecordHeader(
    const base::flat_map<SuitableTrustTokenOrigin, TrustTokenRedemptionRecord>&
        records_per_issuer) {
  net::structured_headers::List header_items;

  for (const auto& issuer_and_record : records_per_issuer) {
    net::structured_headers::Item issuer_item(
        issuer_and_record.first.Serialize(),
        net::structured_headers::Item::ItemType::kStringType);
    net::structured_headers::Item redemption_record_item(
        issuer_and_record.second.body(),
        net::structured_headers::Item::ItemType::kStringType);
    header_items.emplace_back(net::structured_headers::ParameterizedMember(
        std::move(issuer_item), {{kRedemptionRecordHeaderRedemptionRecordKey,
                                  std::move(redemption_record_item)}}));
  }

  return net::structured_headers::SerializeList(std::move(header_items));
}

}  // namespace

TrustTokenRequestSigningHelper::TrustTokenRequestSigningHelper(
    TrustTokenStore* token_store,
    Params params,
    net::NetLogWithSource net_log)
    : token_store_(token_store),
      params_(std::move(params)),
      net_log_(std::move(net_log)) {}

TrustTokenRequestSigningHelper::~TrustTokenRequestSigningHelper() = default;

Params::Params(std::vector<SuitableTrustTokenOrigin> issuers,
               SuitableTrustTokenOrigin toplevel)
    : issuers(std::move(issuers)), toplevel(std::move(toplevel)) {}

Params::Params(SuitableTrustTokenOrigin issuer,
               SuitableTrustTokenOrigin toplevel)
    : toplevel(std::move(toplevel)) {
  issuers.emplace_back(std::move(issuer));
}
Params::~Params() = default;
Params::Params(const Params&) = default;
// The type alias causes a linter false positive.
// NOLINTNEXTLINE(misc-unconventional-assign-operator)
Params& Params::operator=(const Params&) = default;
Params::Params(Params&&) = default;
// NOLINTNEXTLINE(misc-unconventional-assign-operator)
Params& Params::operator=(Params&&) = default;

void TrustTokenRequestSigningHelper::Begin(
    const GURL& url,
    base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                            mojom::TrustTokenOperationStatus)> done) {
#if DCHECK_IS_ON()
  // Add some postcondition checking on return.
  done = base::BindOnce(
      [](base::OnceCallback<void(std::optional<net::HttpRequestHeaders>,
                                 mojom::TrustTokenOperationStatus)> done,
         std::optional<net::HttpRequestHeaders> request_headers,
         mojom::TrustTokenOperationStatus result) {
        DCHECK(request_headers->HasHeader(
            kTrustTokensRequestHeaderSecRedemptionRecord));
        std::move(done).Run(std::move(request_headers), result);
      },
      std::move(done));
#endif  // DCHECK_IS_ON()

  // This class is responsible for adding these headers; callers should not add
  // them.

  net_log_.BeginEvent(
      net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_SIGNING);

  // The numbered comments below are the steps in the "Redemption record
  // attachment and request signing" pseudocode in https://bit.ly/trust-token-dd

  // (Because of the chracteristics of the protocol, this map is expected to
  // have at most ~5 elements.)
  base::flat_map<SuitableTrustTokenOrigin, TrustTokenRedemptionRecord>
      records_per_issuer;

  // 1. For each issuer specified, search storage for a non-expired RR
  // corresponding to that issuer and the request’s initiating top-level origin.
  for (const SuitableTrustTokenOrigin& issuer : params_.issuers) {
    std::optional<TrustTokenRedemptionRecord> maybe_redemption_record =
        token_store_->RetrieveNonstaleRedemptionRecord(issuer,
                                                       params_.toplevel);
    if (!maybe_redemption_record)
      continue;

    records_per_issuer[issuer] = std::move(*maybe_redemption_record);
  }

  net::HttpRequestHeaders request_headers;
  if (records_per_issuer.empty()) {
    AttachRedemptionRecordHeader(request_headers, std::string());

    LogOutcome(net_log_,
               "No RR for any of the given issuers, in the operation's "
               "top-level context");
    std::move(done).Run(std::move(request_headers),
                        mojom::TrustTokenOperationStatus::kOk);
    return;
  }

  // 2. Attach the RRs in a Sec-Redemption-Record header.
  if (std::optional<std::string> maybe_redemption_record_header =
          ConstructRedemptionRecordHeader(records_per_issuer)) {
    AttachRedemptionRecordHeader(request_headers,
                                 std::move(*maybe_redemption_record_header));
  } else {
    AttachRedemptionRecordHeader(request_headers, std::string());

    LogOutcome(net_log_,
               "Unexpected internal error serializing Sec-Redemption-Record"
               " header.");
    std::move(done).Run(std::move(request_headers),
                        mojom::TrustTokenOperationStatus::kOk);
    return;
  }

  request_headers.SetHeader(kTrustTokensSecTrustTokenVersionHeader,
                            kTrustTokensMajorVersion);

  LogOutcome(net_log_, "Success");
  std::move(done).Run(std::move(request_headers),
                      mojom::TrustTokenOperationStatus::kOk);
}

void TrustTokenRequestSigningHelper::Finalize(
    net::HttpResponseHeaders& response_headers,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  return std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

mojom::TrustTokenOperationResultPtr
TrustTokenRequestSigningHelper::CollectOperationResultWithStatus(
    mojom::TrustTokenOperationStatus status) {
  mojom::TrustTokenOperationResultPtr operation_result =
      mojom::TrustTokenOperationResult::New();
  operation_result->status = status;
  operation_result->operation = mojom::TrustTokenOperationType::kRedemption;
  operation_result->top_level_origin = params_.toplevel;
  return operation_result;
}

}  // namespace network
