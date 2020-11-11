// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/test/signed_request_verification_util.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/trust_tokens/ed25519_trust_token_request_signer.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_request_signing_helper.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace network {
namespace test {
namespace {

base::Optional<
    base::flat_map<std::string, net::structured_headers::ParameterizedMember>>
DeserializeSecSignatureHeader(base::StringPiece header) {
  base::Optional<net::structured_headers::Dictionary> maybe_dictionary =
      net::structured_headers::ParseDictionary(header);
  if (!maybe_dictionary)
    return base::nullopt;

  base::flat_map<std::string, net::structured_headers::ParameterizedMember> ret;
  for (const auto& kv : *maybe_dictionary) {
    ret[kv.first] = kv.second;
  }

  return ret;
}

// Given a single issuer's key-value entry from the Sec-Signature
// header and some other data (destination and request headers) from the
// corresponding request, reconstructs the request's canonical signing data
// corresponding to the issuer and verifies the associated signature by calling
// the provided callback.
bool ReconstructSigningDataAndVerifyForIndividualIssuer(
    net::structured_headers::ParameterizedItem& issuer_and_params,
    const GURL& destination,
    const net::HttpRequestHeaders& headers,
    base::RepeatingCallback<bool(base::span<const uint8_t> data,
                                 base::span<const uint8_t> signature,
                                 base::span<const uint8_t> verification_key,
                                 const std::string& sig_alg)> verifier,
    mojom::TrustTokenSignRequestData sign_request_data,
    std::string* error_out,
    std::map<std::string, std::string>* verification_keys_out,
    const std::string& sig_alg) {
  if (!issuer_and_params.item.is_string()) {
    *error_out = "type-unsafe issuer in Sec-Signature header";
    return false;
  }
  std::string issuer = issuer_and_params.item.GetString();  // for debugging

  auto find_key = [&issuer_and_params](base::StringPiece key) {
    return std::find_if(issuer_and_params.params.begin(),
                        issuer_and_params.params.end(),
                        [key](auto& param) { return param.first == key; });
  };

  auto sig_it = find_key("sig");
  if (sig_it == issuer_and_params.params.end()) {
    *error_out = base::ReplaceStringPlaceholders(
        "'sig' element in Sec-Signature header missing for issuer $1", {issuer},
        /*offsets=*/nullptr);
    return false;
  }
  if (!sig_it->second.is_byte_sequence()) {
    *error_out = base::ReplaceStringPlaceholders(
        "'sig' element in Sec-Signature header for issuer $1 is type-unsafe",
        {issuer}, /*offsets=*/nullptr);
    return false;
  }
  // GetString is also the method one uses to get a byte sequence.
  base::StringPiece signature = sig_it->second.GetString();

  auto public_key_it = find_key("public-key");
  if (public_key_it == issuer_and_params.params.end()) {
    *error_out = base::ReplaceStringPlaceholders(
        "'public-key' element in Sec-Signature header missing for issuer $1",
        {issuer}, /*offsets=*/nullptr);
    return false;
  }
  if (!public_key_it->second.is_byte_sequence()) {
    *error_out = base::ReplaceStringPlaceholders(
        "'public-key' element in Sec-Signature header for issuer $1 is "
        "type-unsafe",
        {issuer}, /*offsets=*/nullptr);
    return false;
  }
  base::StringPiece public_key = public_key_it->second.GetString();

  base::Optional<std::vector<uint8_t>> written_reconstructed_cbor =
      TrustTokenRequestCanonicalizer().Canonicalize(
          destination, headers, public_key, sign_request_data);
  if (!written_reconstructed_cbor) {
    *error_out = "Error reconstructing canonical request data";
    return false;
  }

  std::vector<uint8_t> reconstructed_signing_data(
      std::begin(
          TrustTokenRequestSigningHelper::kRequestSigningDomainSeparator),
      std::end(TrustTokenRequestSigningHelper::kRequestSigningDomainSeparator));
  reconstructed_signing_data.insert(reconstructed_signing_data.end(),
                                    written_reconstructed_cbor->begin(),
                                    written_reconstructed_cbor->end());

  if (!verifier) {
    verifier = base::BindRepeating(
        [](base::span<const uint8_t> data, base::span<const uint8_t> signature,
           base::span<const uint8_t> verification_key,
           const std::string& signing_alg) {
          std::unique_ptr<Ed25519TrustTokenRequestSigner> signer =
              std::make_unique<Ed25519TrustTokenRequestSigner>();
          return signer->Verify(data, signature, verification_key) &&
                 signing_alg == signer->GetAlgorithmIdentifier();
        });
  }

  if (!verifier.Run(base::make_span(reconstructed_signing_data),
                    base::as_bytes(base::make_span(signature)),
                    base::as_bytes(base::make_span(public_key)), sig_alg)) {
    *error_out = "Error verifying signature";
    return false;
  }

  if (verification_keys_out)
    verification_keys_out->emplace(issuer, std::string(public_key));

  return true;
}

using SignatureHeaderMap =
    base::flat_map<std::string, net::structured_headers::ParameterizedMember>;

bool ExtractSignRequestDataFromSignatureHeaderMap(
    const SignatureHeaderMap& map,
    mojom::TrustTokenSignRequestData* sign_request_data_out,
    std::string* error_out) {
  auto it = map.find("sign-request-data");
  if (it == map.end()) {
    *error_out =
        "Missing 'sign-request-data' element in the Sec-Signature header";
    return false;
  }
  if (!it->second.member.front().item.is_token()) {
    *error_out =
        "'sign-request-data' element in Sec-Signature header is type-unsafe";
    return false;
  }

  // GetString is also the method one uses to get a token.
  base::StringPiece sign_request_data =
      it->second.member.front().item.GetString();

  if (sign_request_data != "headers-only" && sign_request_data != "include") {
    *error_out =
        "'sign-request-data' element in Sec-Signature header had a bad "
        "value: " +
        std::string(sign_request_data);
    return false;
  }

  *sign_request_data_out = (sign_request_data == "headers-only")
                               ? mojom::TrustTokenSignRequestData::kHeadersOnly
                               : mojom::TrustTokenSignRequestData::kInclude;
  return true;
}

bool ExtractIssuersAndParametersFromSignatureHeaderMap(
    const SignatureHeaderMap& map,
    std::vector<net::structured_headers::ParameterizedItem>*
        issuers_and_parameters_out,
    std::string* error_out) {
  auto it = map.find("signatures");
  if (it == map.end()) {
    *error_out = "Missing 'signatures' element in the Sec-Signature header";
    return false;
  }
  if (!it->second.member_is_inner_list) {
    *error_out = "'signatures' element is not an inner list";
    return false;
  }

  *issuers_and_parameters_out = it->second.member;
  return true;
}

bool ExtractSigningAlgorithmIdentifierFromSignatureHeaderMap(
    const SignatureHeaderMap& map,
    std::string* sig_alg_out,
    std::string* error_out) {
  auto it = map.find("alg");
  if (it == map.end()) {
    *error_out = "Missing 'alg' element in the Sec-Signature header";
    return false;
  }

  if (it->second.member_is_inner_list) {
    *error_out = "'alg' element should not be a list";
    return false;
  }

  *sig_alg_out = it->second.member.front().item.GetString();
  return true;
}

bool ValidateSignatureHeaderMapAndExtractFields(
    const SignatureHeaderMap& map,
    std::vector<net::structured_headers::ParameterizedItem>*
        issuers_and_parameters_out,
    mojom::TrustTokenSignRequestData* sign_request_data_out,
    std::string* sig_alg_out,
    std::string* error_out) {
  if (map.size() != 3) {
    *error_out = "Unexpected number of members in Sec-Signature header map";
    return false;
  }

  if (!ExtractSignRequestDataFromSignatureHeaderMap(map, sign_request_data_out,
                                                    error_out)) {
    return false;
  }

  if (!ExtractIssuersAndParametersFromSignatureHeaderMap(
          map, issuers_and_parameters_out, error_out)) {
    return false;
  }

  if (!ExtractSigningAlgorithmIdentifierFromSignatureHeaderMap(map, sig_alg_out,
                                                               error_out)) {
    return false;
  }

  return true;
}

}  // namespace

bool ReconstructSigningDataAndVerifySignatures(
    const GURL& destination,
    const net::HttpRequestHeaders& headers,
    base::RepeatingCallback<bool(base::span<const uint8_t> data,
                                 base::span<const uint8_t> signature,
                                 base::span<const uint8_t> verification_key,
                                 const std::string& sig_alg)> verifier,
    std::string* error_out,
    std::map<std::string, std::string>* verification_keys_out,
    mojom::TrustTokenSignRequestData* sign_request_data_out) {
  // Make it possible to set the error without needing to check for
  // |error_out|'s presence.
  std::string dummy_error;
  if (!error_out)
    error_out = &dummy_error;

  std::string signature_header;
  if (!headers.GetHeader(kTrustTokensRequestHeaderSecSignature,
                         &signature_header)) {
    *error_out = "Missing Sec-Signature header";
    return false;
  }

  base::Optional<
      base::flat_map<std::string, net::structured_headers::ParameterizedMember>>
      signature_header_map = DeserializeSecSignatureHeader(signature_header);
  if (!signature_header_map) {
    *error_out = "Malformed Sec-Signature header";
    return false;
  }

  std::vector<net::structured_headers::ParameterizedItem>
      issuers_and_parameters;
  mojom::TrustTokenSignRequestData sign_request_data;
  std::string sig_alg;
  if (!ValidateSignatureHeaderMapAndExtractFields(
          *signature_header_map, &issuers_and_parameters, &sign_request_data,
          &sig_alg, error_out)) {
    return false;
  }
  if (sign_request_data_out)
    *sign_request_data_out = sign_request_data;

  for (net::structured_headers::ParameterizedItem& issuer_and_parameters :
       issuers_and_parameters) {
    // ReconstructSigningDataAndVerifyForIndividualIssuer will populate
    // |error_out| on failure.
    if (!ReconstructSigningDataAndVerifyForIndividualIssuer(
            issuer_and_parameters, destination, headers, verifier,
            sign_request_data, error_out, verification_keys_out, sig_alg)) {
      return false;
    }
  }

  return true;
}

bool ExtractRedemptionRecordsFromHeader(
    base::StringPiece sec_redemption_record_header,
    std::map<SuitableTrustTokenOrigin, std::string>*
        redemption_records_per_issuer_out,
    std::string* error_out) {
  base::Optional<net::structured_headers::List> maybe_list =
      net::structured_headers::ParseList(sec_redemption_record_header);

  std::string dummy;
  if (!error_out)
    error_out = &dummy;

  if (!maybe_list) {
    *error_out = "Header wasn't a valid Structured Headers list";
    return false;
  }

  for (auto& issuer_and_params : *maybe_list) {
    net::structured_headers::Item& issuer_item =
        issuer_and_params.member.front().item;
    if (!issuer_item.is_string()) {
      *error_out = "Non-string item in the RR header's list";
      return false;
    }

    const net::structured_headers::Parameters& params_for_issuer =
        issuer_and_params.params;
    if (params_for_issuer.size() != 1) {
      *error_out =
          base::StrCat({"Unexpected number of parameters for RR header list "
                        "item; expected 1 parameter but there were ",
                        base::NumberToString(params_for_issuer.size())});
      return false;
    }
    if (params_for_issuer.front().first != "redemption-record") {
      *error_out = base::ReplaceStringPlaceholders(
          "Unexpected parameter key $1 for RR header list item",
          {params_for_issuer.front().first}, /*offsets=*/nullptr);
      return false;
    }

    const net::structured_headers::Item& redemption_record_item =
        params_for_issuer.front().second;
    if (!redemption_record_item.is_byte_sequence()) {
      *error_out = "Unexpected parameter value type for RR header list item";
      return false;
    }

    base::Optional<SuitableTrustTokenOrigin> maybe_issuer =
        SuitableTrustTokenOrigin::Create(GURL(issuer_item.GetString()));
    if (!maybe_issuer) {
      *error_out = "Unsuitable Trust Tokens issuer origin in RR header item";
      return false;
    }

    // GetString also gets a byte sequence.
    redemption_records_per_issuer_out->emplace(
        std::move(*maybe_issuer), redemption_record_item.GetString());
  }
  return true;
}

}  // namespace test
}  // namespace network
