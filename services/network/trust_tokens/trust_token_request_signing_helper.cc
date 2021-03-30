// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

#include <iterator>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "net/http/structured_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "url/url_constants.h"

namespace network {

namespace {

void LogOutcome(const net::NetLogWithSource& log, base::StringPiece outcome) {
  log.EndEvent(net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_SIGNING,
               [outcome]() {
                 base::Value ret(base::Value::Type::DICTIONARY);
                 ret.SetStringKey("outcome", outcome);
                 return ret;
               });
}

}  // namespace

namespace internal {

// Parse the Signed-Headers input header as a Structured Headers Draft 15 list
// of "tokens" (unquoted strings with a constrained alphabet).
base::Optional<std::vector<std::string>> ParseTrustTokenSignedHeadersHeader(
    base::StringPiece header) {
  base::Optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(header);
  if (!maybe_list)
    return base::nullopt;
  std::vector<std::string> ret;
  for (const net::structured_headers::ParameterizedMember&
           parameterized_member : *maybe_list) {
    if (!parameterized_member.params.empty() ||
        parameterized_member.member.size() != 1) {
      return base::nullopt;
    }
    const net::structured_headers::ParameterizedItem& parameterized_item =
        parameterized_member.member.front();
    if (!parameterized_item.params.empty())
      return base::nullopt;
    if (!parameterized_item.item.is_token())
      return base::nullopt;
    ret.push_back(parameterized_item.item.GetString());
  }
  return ret;
}

}  // namespace internal

const char* const TrustTokenRequestSigningHelper::kSignableRequestHeaders[]{
    kTrustTokensRequestHeaderSecRedemptionRecord,
    kTrustTokensRequestHeaderSecTime,
    kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData,
};

constexpr char
    TrustTokenRequestSigningHelper::kCanonicalizedRequestDataDestinationKey[];
constexpr char
    TrustTokenRequestSigningHelper::kCanonicalizedRequestDataPublicKeyKey[];
constexpr uint8_t
    TrustTokenRequestSigningHelper::kRequestSigningDomainSeparator[];

namespace {

using Params = TrustTokenRequestSigningHelper::Params;

// Constants for keys and values in the generated headers:
const char kSignatureHeaderSignRequestDataIncludeValue[] = "include";
const char kSignatureHeaderSignRequestDataHeadersOnlyValue[] = "headers-only";
const char kSignatureHeaderSignaturesKey[] = "signatures";
const char kSignatureHeaderSignRequestDataKey[] = "sign-request-data";
const char kSignatureHeaderAlgorithmKey[] = "alg";
const char kSignatureHeaderPublicKeyKey[] = "public-key";
const char kSignatureHeaderSignatureKey[] = "sig";
const char kRedemptionRecordHeaderRedemptionRecordKey[] = "redemption-record";

std::vector<std::string> Lowercase(std::vector<std::string> in) {
  for (std::string& str : in) {
    for (auto& ch : str) {
      ch = base::ToLowerASCII(ch);
    }
  }

  return in;
}

// In order to check whether all of the header names given by the client are
// signable, perform a single initial computation of the lower-cased versions
// of |kSignableRequestHeaders|.
const base::flat_set<std::string>& LowercaseSignableHeaders() {
  static base::NoDestructor<base::flat_set<std::string>>
      kLowercaseSignableHeaders{Lowercase(
          {std::begin(TrustTokenRequestSigningHelper::kSignableRequestHeaders),
           std::end(TrustTokenRequestSigningHelper::kSignableRequestHeaders)})};
  return *kLowercaseSignableHeaders;
}

// Attempts to combine the (comma-delimited) header names in |request|'s
// Signed-Headers header, if any, and the members of |additional_headers|.
//
// Returns nullopt, and removes |request|'s Signed-Headers header, if any
// provided header name is not present in the signable headers allowlist
// TrustTokenRequestSigningHelper::kSignableRequestHeaders.
//
// Otherwise:
// - updates |request|'s Signed-Headers header to contain the union of the
// lower-cased members of |additional_headers| and the lower-cased elements of
// |request|'s previous header value; and
// - returns the list of these header names.
base::Optional<std::vector<std::string>>
GetHeadersToSignAndUpdateSignedHeadersHeader(
    net::URLRequest* request,
    const std::vector<std::string>& additional_headers) {
  std::string signed_headers_header;
  ignore_result(request->extra_request_headers().GetHeader(
      kTrustTokensRequestHeaderSignedHeaders, &signed_headers_header));

  // Because of the characteristics of the protocol, there are expected to be
  // roughly 2-5 total headers to sign.
  base::flat_set<std::string> deduped_lowercase_headers_to_sign(
      Lowercase(additional_headers));

  base::Optional<std::vector<std::string>> maybe_parsed_header_names =
      internal::ParseTrustTokenSignedHeadersHeader(signed_headers_header);

  // Remove the Signed-Headers header:
  // - On failure, or on success with no headers to sign, this will stay removed
  // in order to denote that no headers are being signed.
  // - On success, it will be added back to the request.
  request->RemoveRequestHeaderByName(kTrustTokensRequestHeaderSignedHeaders);

  // Fail if the request's Signed-Headers header existed but failed to parse.
  if (!maybe_parsed_header_names)
    return base::nullopt;

  for (const std::string& header_name : Lowercase(*maybe_parsed_header_names))
    deduped_lowercase_headers_to_sign.insert(header_name);

  // If there are no headers to sign, don't bother readding the Signed-Headers
  // header.
  if (deduped_lowercase_headers_to_sign.empty())
    return std::vector<std::string>();

  if (!base::ranges::includes(LowercaseSignableHeaders(),
                              deduped_lowercase_headers_to_sign)) {
    return base::nullopt;
  }

  std::vector<std::string> out(
      std::make_move_iterator(deduped_lowercase_headers_to_sign.begin()),
      std::make_move_iterator(deduped_lowercase_headers_to_sign.end()));

  request->SetExtraRequestHeaderByName(kTrustTokensRequestHeaderSignedHeaders,
                                       base::JoinString(out, ","),
                                       /*overwrite=*/true);
  return out;
}

void AttachRedemptionRecordHeader(net::URLRequest* request, std::string value) {
  request->SetExtraRequestHeaderByName(
      kTrustTokensRequestHeaderSecRedemptionRecord, value,
      /*overwrite=*/true);
}

// Builds a Trust Tokens redemption record header, which is logically an
// issuer-to-RR map but implemented as a Structured Headers Draft 15
// parameterized list (essentially a list where each member has an associated
// dictionary).
base::Optional<std::string> ConstructRedemptionRecordHeader(
    const base::flat_map<SuitableTrustTokenOrigin, TrustTokenRedemptionRecord>&
        records_per_issuer) {
  net::structured_headers::List header_items;

  for (const auto& issuer_and_record : records_per_issuer) {
    net::structured_headers::Item issuer_item(
        issuer_and_record.first.Serialize(),
        net::structured_headers::Item::ItemType::kStringType);
    net::structured_headers::Item redemption_record_item(
        issuer_and_record.second.body(),
        net::structured_headers::Item::ItemType::kByteSequenceType);
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
    std::unique_ptr<Signer> signer,
    std::unique_ptr<TrustTokenRequestCanonicalizer> canonicalizer,
    net::NetLogWithSource net_log)
    : token_store_(token_store),
      params_(std::move(params)),
      signer_(std::move(signer)),
      canonicalizer_(std::move(canonicalizer)),
      net_log_(std::move(net_log)) {}

TrustTokenRequestSigningHelper::~TrustTokenRequestSigningHelper() = default;

Params::Params(
    std::vector<SuitableTrustTokenOrigin> issuers,
    SuitableTrustTokenOrigin toplevel,
    std::vector<std::string> additional_headers_to_sign,
    bool should_add_timestamp,
    mojom::TrustTokenSignRequestData sign_request_data,
    base::Optional<std::string> possibly_unsafe_additional_signing_data)
    : issuers(std::move(issuers)),
      toplevel(std::move(toplevel)),
      additional_headers_to_sign(std::move(additional_headers_to_sign)),
      should_add_timestamp(should_add_timestamp),
      sign_request_data(sign_request_data),
      possibly_unsafe_additional_signing_data(
          possibly_unsafe_additional_signing_data) {}

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
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  DCHECK(request);
#if DCHECK_IS_ON()
  // Add some postcondition checking on return.
  done = base::BindOnce(
      [](net::URLRequest* request,
         base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
         mojom::TrustTokenOperationStatus result) {
        const auto& headers = request->extra_request_headers();

        std::string rr_header;
        DCHECK(headers.GetHeader(kTrustTokensRequestHeaderSecRedemptionRecord,
                                 &rr_header));
        if (rr_header.empty()) {
          DCHECK(!headers.HasHeader(kTrustTokensRequestHeaderSecTime));
          DCHECK(!headers.HasHeader(kTrustTokensRequestHeaderSecSignature));
          DCHECK(!headers.HasHeader(kTrustTokensRequestHeaderSignedHeaders));
        }
        std::move(done).Run(result);
      },
      request, std::move(done));
#endif  // DCHECK_IS_ON()

  // This class is responsible for adding these headers; callers should not add
  // them.
  DCHECK(!request->extra_request_headers().HasHeader(
      kTrustTokensRequestHeaderSecRedemptionRecord));
  DCHECK(!request->extra_request_headers().HasHeader(
      kTrustTokensRequestHeaderSecTime));
  DCHECK(!request->extra_request_headers().HasHeader(
      kTrustTokensRequestHeaderSecSignature));

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
    base::Optional<TrustTokenRedemptionRecord> maybe_redemption_record =
        token_store_->RetrieveNonstaleRedemptionRecord(issuer,
                                                       params_.toplevel);
    if (!maybe_redemption_record)
      continue;

    records_per_issuer[issuer] = std::move(*maybe_redemption_record);
  }

  if (records_per_issuer.empty()) {
    AttachRedemptionRecordHeader(request, std::string());

    LogOutcome(net_log_,
               "No RR for any of the given issuers, in the operation's "
               "top-level context");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
    return;
  }

  // 2.a. If the request’s additionalSigningData argument is nonempty, add a
  // Sec-Trust-Tokens-Additional-Signing-Data header to the request with its
  // value equal to that of the request’s additionalSigningData argument.
  if (params_.possibly_unsafe_additional_signing_data) {
    // 2.a.i. If it is longer than 2048 bytes, raise an error.
    if (params_.possibly_unsafe_additional_signing_data->size() >
        kTrustTokenAdditionalSigningDataMaxSizeBytes) {
      LogOutcome(net_log_, "Overly long additionalSigningData");

      AttachRedemptionRecordHeader(request, std::string());
      std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
      return;
    }

    // 2.a.ii. If it is not a valid HTTP header value (contains \r, \n, or \0),
    // raise an error.
    if (!net::HttpUtil::IsValidHeaderValue(
            *params_.possibly_unsafe_additional_signing_data)) {
      LogOutcome(net_log_,
                 "additionalSigningData was not a valid HTTP header value");

      AttachRedemptionRecordHeader(request, std::string());
      std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
      return;
    }

    // |request| is guaranteed to not have
    // kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData because
    // network::TrustTokenRequestHeaders() contains this header name, so the
    // request would have been rejected as failing a precondition if the header
    // were present.
    DCHECK(!request->extra_request_headers().HasHeader(
        kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData));

    // 2.a.iii. Add a Sec-Trust-Tokens-Additional-Signing-Data header to the
    // request with its value equal to that of the request’s
    // additionalSigningData argument.
    request->SetExtraRequestHeaderByName(
        kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData,
        *params_.possibly_unsafe_additional_signing_data,
        /*overwrite=*/true);

    params_.additional_headers_to_sign.push_back(
        kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData);
  }

  // 2.b. Merge the additionalRequestHeaders Fetch parameter’s contents into the
  // request’s Signed-Headers header (creating a header if previously absent).
  // If the request has a Sec-Trust-Tokens-Additional-Signing-Data header,
  // append “Sec-Trust-Tokens-Additional-Signing-Data” to the request’s
  // Signed-Headers header.
  base::Optional<std::vector<std::string>> maybe_headers_to_sign =
      GetHeadersToSignAndUpdateSignedHeadersHeader(
          request, params_.additional_headers_to_sign);

  if (!maybe_headers_to_sign) {
    AttachRedemptionRecordHeader(request, std::string());

    LogOutcome(net_log_,
               "Unsignable header specified in Signed-Headers "
               "header or additionalSignedHeaders arg");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
    return;
  }

  // 2.c. Attach the RRs in a Sec-Redemption-Record header.
  if (base::Optional<std::string> maybe_redemption_record_header =
          ConstructRedemptionRecordHeader(records_per_issuer)) {
    AttachRedemptionRecordHeader(request,
                                 std::move(*maybe_redemption_record_header));
  } else {
    AttachRedemptionRecordHeader(request, std::string());

    LogOutcome(net_log_,
               "Unexpected internal error serializing Sec-Redemption-Record"
               " header.");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
    return;
  }

  // 2.d. If specified by the request’s includeTimestampHeader parameter, add a
  // Sec-Time header containing a high-resolution timestamp, encoded in ISO8601.
  if (params_.should_add_timestamp) {
    request->SetExtraRequestHeaderByName(kTrustTokensRequestHeaderSecTime,
                                         base::TimeToISO8601(base::Time::Now()),
                                         /*overwrite=*/true);
  }

  if (params_.sign_request_data == mojom::TrustTokenSignRequestData::kOmit) {
    LogOutcome(net_log_, "Success (sign-request-data='omit')");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
    return;
  }

  // 2.e. If the request’s signRequestData Fetch parameter is not “omit”, follow
  // the steps in the Computing an outgoing request’s signatures section to add
  // a Sec-Signature header containing, for each issuer, a signature over some
  // of the request’s data (e.g. a subset of its headers) generated using a key
  // bound to a prior redemption against that issuer.
  base::flat_map<SuitableTrustTokenOrigin, std::vector<uint8_t>>
      signatures_per_issuer;

  for (const auto& issuer_and_record : records_per_issuer) {
    if (base::Optional<std::vector<uint8_t>> maybe_signature = GetSignature(
            request, issuer_and_record.second, *maybe_headers_to_sign)) {
      signatures_per_issuer[issuer_and_record.first] =
          std::move(*maybe_signature);
    } else {
      // Failure isn't likely and would mean that the signing key---which we
      // generate ourselves, during redemption---somehow got corrupted, or there
      // was some kind of internal error generating the signature in the
      // underlying cryptography library.
      net_log_.AddEntry(
          net::NetLogEventType::TRUST_TOKEN_OPERATION_BEGIN_SIGNING,
          net::NetLogEventPhase::NONE,
          [&issuer_and_record](net::NetLogCaptureMode mode) {
            base::Value ret(base::Value::Type::DICTIONARY);
            ret.SetStringPath("failed_signing_params.issuer",
                              issuer_and_record.first.Serialize());
            if (net::NetLogCaptureIncludesSensitive(mode)) {
              ret.SetStringPath("failed_signing_params.key",
                                issuer_and_record.second.signing_key());
            }
            return ret;
          });
    }
  }

  if (base::Optional<std::string> maybe_signature_header =
          BuildSignatureHeaderIfAtLeastOneSignatureIsPresent(
              records_per_issuer, signatures_per_issuer)) {
    request->SetExtraRequestHeaderByName(kTrustTokensRequestHeaderSecSignature,
                                         std::move(*maybe_signature_header),
                                         /*overwrite=*/true);
  } else {
    AttachRedemptionRecordHeader(request, std::string());
    request->RemoveRequestHeaderByName(kTrustTokensRequestHeaderSecTime);
    request->RemoveRequestHeaderByName(kTrustTokensRequestHeaderSignedHeaders);

    LogOutcome(net_log_,
               "Internal error serializing signature header, or generating all "
               "issuers' signatures failed.");
    std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
    return;
  }

  LogOutcome(net_log_, "Success");
  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

void TrustTokenRequestSigningHelper::Finalize(
    mojom::URLResponseHead* response,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  return std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

namespace {

// Given a redemption record and a signature bytestring, returns a {
//   "public-key": <public key>,
//   "sig": <signature>
// }
// nested map, corresponding to a single entry in the Sec-Signature header's
// top-level list.
net::structured_headers::Parameters ConstructKeyAndSignaturePair(
    const TrustTokenRedemptionRecord& redemption_record,
    base::span<const uint8_t> signature_bytes) {
  net::structured_headers::Item public_key(
      redemption_record.public_key(),
      net::structured_headers::Item::ItemType::kByteSequenceType);
  net::structured_headers::Item signature(net::structured_headers::Item(
      std::string(reinterpret_cast<const char*>(signature_bytes.data()),
                  signature_bytes.size()),
      net::structured_headers::Item::ItemType::kByteSequenceType));

  return {{kSignatureHeaderPublicKeyKey, std::move(public_key)},
          {kSignatureHeaderSignatureKey, std::move(signature)}};
}

}  // namespace

base::Optional<std::string> TrustTokenRequestSigningHelper::
    BuildSignatureHeaderIfAtLeastOneSignatureIsPresent(
        const base::flat_map<SuitableTrustTokenOrigin,
                             TrustTokenRedemptionRecord>& records_per_issuer,
        const base::flat_map<SuitableTrustTokenOrigin, std::vector<uint8_t>>&
            signatures_per_issuer) {
  if (signatures_per_issuer.empty())
    return base::nullopt;

  net::structured_headers::Dictionary header_items;

  header_items[kSignatureHeaderAlgorithmKey] =
      net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              signer_->GetAlgorithmIdentifier(),
              net::structured_headers::Item::ItemType::kStringType),
              {});

  std::vector<net::structured_headers::ParameterizedItem> keys_and_signatures;
  for (const auto& kv : signatures_per_issuer) {
    const SuitableTrustTokenOrigin& issuer = kv.first;
    const std::vector<uint8_t>& signature = kv.second;

    keys_and_signatures.emplace_back(net::structured_headers::ParameterizedItem(
        net::structured_headers::Item(
            issuer.Serialize(),
            net::structured_headers::Item::ItemType::kStringType),
        // records_per_issuer is guaranteed to have all of the keys that
        // signatures_per_issuer does, so using |at| is safe:
        ConstructKeyAndSignaturePair(records_per_issuer.at(issuer),
                                     signature)));
  }

  header_items[kSignatureHeaderSignaturesKey] =
      net::structured_headers::ParameterizedMember(
          std::move(keys_and_signatures),
          net::structured_headers::Parameters());

  // A value of kOmit denotes not wanting the request signed at all, so it'd be
  // a caller error if we were trying to sign the request with it set.
  DCHECK_NE(params_.sign_request_data, mojom::TrustTokenSignRequestData::kOmit);

  const char* sign_request_data_value = "";
  switch (params_.sign_request_data) {
    case mojom::TrustTokenSignRequestData::kInclude:
      sign_request_data_value = kSignatureHeaderSignRequestDataIncludeValue;
      break;
    case mojom::TrustTokenSignRequestData::kHeadersOnly:
      sign_request_data_value = kSignatureHeaderSignRequestDataHeadersOnlyValue;
      break;
    case mojom::TrustTokenSignRequestData::kOmit:
      NOTREACHED();  // "omit" handled (far) above.
      break;
  }

  header_items[kSignatureHeaderSignRequestDataKey] =
      net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              sign_request_data_value,
              net::structured_headers::Item::ItemType::kTokenType),
          {});

  return net::structured_headers::SerializeDictionary(header_items);
}

base::Optional<std::vector<uint8_t>>
TrustTokenRequestSigningHelper::GetSignature(
    net::URLRequest* request,
    const TrustTokenRedemptionRecord& redemption_record,
    const std::vector<std::string>& headers_to_sign) {
  // (This follows the normative pseudocode, labeled "signature
  // generation," in the Trust Tokens design doc.)
  //
  // 1. Generate a CBOR-encoded dictionary, the canonical request data.
  // 2. Sign the concatenation of the major protocol version and the
  // CBOR-encoded dictionary. (The domain separator string allows versioning
  // otherwise-forward-compatible protocol structures, which is useful in case
  // the semantics change across versions.)

  base::Optional<std::vector<uint8_t>> maybe_request_in_cbor =
      canonicalizer_->Canonicalize(
          request->url(), request->extra_request_headers(),
          redemption_record.public_key(), params_.sign_request_data);

  if (!maybe_request_in_cbor)
    return base::nullopt;

  // kRequestSigningDomainSeparator is an explicitly-specified char array, not
  // a string literal, so this will, as intended, not include a null terminator.
  std::vector<uint8_t> signing_data(std::begin(kRequestSigningDomainSeparator),
                                    std::end(kRequestSigningDomainSeparator));
  signing_data.insert(signing_data.end(), maybe_request_in_cbor->begin(),
                      maybe_request_in_cbor->end());

  base::span<const uint8_t> key_bytes =
      base::as_bytes(base::make_span(redemption_record.signing_key()));
  return signer_->Sign(key_bytes, base::make_span(signing_data));
}

mojom::TrustTokenOperationResultPtr
TrustTokenRequestSigningHelper::CollectOperationResultWithStatus(
    mojom::TrustTokenOperationStatus status) {
  mojom::TrustTokenOperationResultPtr operation_result =
      mojom::TrustTokenOperationResult::New();
  operation_result->status = status;
  operation_result->type = mojom::TrustTokenOperationType::kRedemption;
  operation_result->top_level_origin = params_.toplevel;
  return operation_result;
}

}  // namespace network
