// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/two_qwac.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "net/cert/x509_util.h"

namespace net {

Jades2QwacHeader::Jades2QwacHeader() = default;
Jades2QwacHeader::Jades2QwacHeader(const Jades2QwacHeader& other) = default;
Jades2QwacHeader::Jades2QwacHeader(Jades2QwacHeader&& other) = default;
Jades2QwacHeader::~Jades2QwacHeader() = default;

namespace {

std::optional<Jades2QwacHeader> ParseJades2QwacHeader(
    std::string_view header_string) {
  Jades2QwacHeader parsed_header;
  // The header of a JWS is a JSON-encoded object (RFC 7515, section 4).
  std::optional<base::Value> header_value =
      base::JSONReader::Read(header_string, base::JSON_PARSE_RFC);
  if (!header_value.has_value() || !header_value->is_dict()) {
    return std::nullopt;
  }
  base::Value::Dict& header = header_value->GetDict();

  // "alg" (Algorithm) parameter - RFC 7515, section 4.1.1
  std::string* alg = header.FindString("alg");
  if (!alg || *alg == "") {
    return std::nullopt;
  }
  parsed_header.sig_alg = *alg;
  header.Remove("alg");
  // TODO(crbug.com/392929826): process alg (check that it matches the alg in
  // x5c).

  // "kid" (Key ID) parameter - RFC 7515, section 4.1.4
  //
  // The Key ID can be of any type and is used to identify the key used for
  // signing. In this profile, the key used to verify the signature will be
  // found in the "x5c" parameter, so the "kid" is useless to us and is ignored.
  header.Remove("kid");

  // "cty" (Content Type) parameter - RFC 7515, section 4.1.10
  //
  // ETSI TS 119 411-5 V2.1.1 requires the "cty" parameter to be
  // "TLS-Certificate-Binding-v1".
  std::string* cty = header.FindString("cty");
  if (!cty || *cty != "TLS-Certificate-Binding-v1") {
    return std::nullopt;
  }
  header.Remove("cty");

  // "x5t#S256" (X.509 Certificate SHA-256 Thumbprint) parameter (RFC 7515,
  // section 4.1.8) is the base64url-encoded SHA-256 thumbprint of the
  // DER encoding of the X.509 certificate used to sign the JWS. This value is
  // not needed to verify the signature (the leaf cert of the "x5c" parameter is
  // the signing cert), and it is optional according to RFC 7515, so we ignore
  // it.
  if (header.FindString("x5t#S256")) {
    header.Remove("x5t#S256");
  }

  // "x5c" (X.509 Certificate Chain) header - RFC 7515 section 4.1.6
  base::ListValue* x5c_list = header.FindList("x5c");
  if (!x5c_list) {
    return std::nullopt;
  }

  size_t i = 0;
  bssl::UniquePtr<CRYPTO_BUFFER> leaf;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (const base::Value& cert_value : *x5c_list) {
    // RFC 7515 section 4.1.6:
    // "Each string in the array is a base64-encoded (not base64url-encoded) DER
    // PKIX certificate value."
    if (!cert_value.is_string()) {
      return std::nullopt;
    }
    auto cert_bytes = base::Base64Decode(cert_value.GetString());
    if (!cert_bytes.has_value()) {
      return std::nullopt;
    }
    auto buf = x509_util::CreateCryptoBuffer(*cert_bytes);
    if (i == 0) {
      leaf = std::move(buf);
    } else {
      intermediates.emplace_back(std::move(buf));
    }
    i++;
  }
  parsed_header.two_qwac_cert = X509Certificate::CreateFromBuffer(
      std::move(leaf), std::move(intermediates));
  if (!parsed_header.two_qwac_cert) {
    return std::nullopt;
  }
  header.Remove("x5c");

  // "iat" header. TS 119 182-1 section 5.1.11 defines this header parameter to
  // be almost the same as RFC 7519's JWT "iat" claim. Despite TS 119 182-1
  // citing RFC 7519 as the definition for this header parameter, JWS header
  // parameters and JWT claims are not the same thing. In any case, ETSI defines
  // this header to be an integer representing the claimed signing time.
  //
  // I see no indication in TS 119 411-5 that "iat" is required to be present,
  // and RFC 7519 specifies it as optional. Further, I haven't yet found an
  // indication as to how one would interpret and apply this field in signature
  // validation, so I'm ignoring it.
  if (header.FindInt("iat")) {
    header.Remove("iat");
  }

  // "exp" header. TS 119 411-5 Annex B defines this as the expiry date of the
  // binding, and like TS 119 182-1 for the "iat" header, incorrectly cites RFC
  // 7519's claim definition of the field (section 4.1.4). Unlike the ETSI
  // specification for "iat" that restricts its NumericDate type to an integer,
  // we only have the RFC 7519 definition of "exp" to use, which defines
  // NumericDate as a JSON numeric value. RFC 7159 allows JSON numeric values to
  // contain a fraction part.
  //
  // Like the "iat" header, TS 119 411-5 does not require the presence of "exp",
  // RFC 7519 specifies it as optional, and there is no indication in any ETSI
  // spec on how this field would affect signature validation, so it is ignored.
  if (header.FindDouble("exp")) {
    header.Remove("exp");
  }

  // "sigD" header - ETSI TS 119 182-1 section 5.2.8, with additional
  // requirements specified in ETSI TS 119 411-5 Annex B. This parameter is a
  // JSON object and is required to be present.
  base::DictValue* sig_d = header.FindDict("sigD");
  if (!sig_d) {
    return std::nullopt;
  }

  // The sigD header must have a "mId" (mechanism ID) of
  // "http://uri.etsi.org/19182/ObjectIdByURIHash". (ETSI TS 119 411-5 Annex B.)
  std::string* m_id = sig_d->FindString("mId");
  if (!m_id || *m_id != "http://uri.etsi.org/19182/ObjectIdByURIHash") {
    return std::nullopt;
  }
  sig_d->Remove("mId");

  // The sigD header must have a "pars" member, which is a list of strings. We
  // don't care about the contents of this list, but its size must match that of
  // "hashV". (ETSI 119 182-1 clause 5.2.8.)
  const base::ListValue* pars = sig_d->FindList("pars");
  if (!pars) {
    return std::nullopt;
  }
  size_t bound_cert_count = pars->size();
  for (const base::Value& par : *pars) {
    if (!par.is_string()) {
      return std::nullopt;
    }
  }
  sig_d->Remove("pars");

  // The sigD header must have a "hashM" member (TS 119 182-1
  // section 5.2.8.3.3), which is a string identifying the hashing algorithm
  // used for the "hashV" member. ETSI TS 119 411-5 only requires that S256,
  // S384, and S512 be supported.
  std::string* hash_m = sig_d->FindString("hashM");
  if (!hash_m) {
    return std::nullopt;
  }
  if (*hash_m == "S256") {
    parsed_header.hash_alg = crypto::hash::kSha256;
  } else if (*hash_m == "S384") {
    // TODO(crbug.com/392929826): add SHA-384 to crypto/hash.h.
    return std::nullopt;
  } else if (*hash_m == "S512") {
    parsed_header.hash_alg = crypto::hash::kSha512;
  } else {
    // Unsupported hashing algorithm.
    return std::nullopt;
  }
  sig_d->Remove("hashM");

  // The sigD header must have a "hashV" member, which is a list of
  // base64url-encoded digest values of the base64url-encoded data objects.
  // (ETSI TS 119 182-1 clause 5.2.8. The "b64" header parameter is absent, so
  // the digest is computed over the base64url-encoded data object instead of
  // computed directly over the data object.)
  const base::ListValue* hash_v = sig_d->FindList("hashV");
  if (!hash_v) {
    return std::nullopt;
  }
  if (hash_v->size() != bound_cert_count) {
    return std::nullopt;
  }
  parsed_header.bound_cert_hashes.reserve(bound_cert_count);
  for (const base::Value& hash_value : *hash_v) {
    const std::string* hash_b64url = hash_value.GetIfString();
    if (!hash_b64url) {
      return std::nullopt;
    }
    // ETSI TS 119 182-1 fails to specify the definition of "base64url-encoded".
    // Given that other uses of base64url encoding come from the JWS spec, and
    // JWS disallows padding in its base64url encoding, we disallow it here as
    // well.
    auto hash = base::Base64UrlDecode(
        *hash_b64url, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
    if (!hash.has_value()) {
      return std::nullopt;
    }
    parsed_header.bound_cert_hashes.emplace_back(std::move(*hash));
  }
  sig_d->Remove("hashV");

  // Given the mId used, the sigD header may have a "ctys" member (TS 119 182-1
  // clause 5.2.8.3.3), with semantics and syntax as specified in clause
  // 5.2.8.1. Clause 5.2.8.1 defines the "ctys" member's syntax to be an array
  // of strings. This array has the same length as the "pars" (and "hashV")
  // array, and each element is the content type (RFC 7515 section 4.1.10) of
  // the data object referred to by the value in "pars" at the same index.
  // RFC 7515 specifies that the content type parameter is ignored by JWS
  // implementations and processing of it is performed by the JWS application.
  // Since neither ETSI TS 119 182-1 nor TS 119 411-5 provide guidance on the
  // content type used for the individual data objects, this implementation has
  // no opinion on the stated content types.
  const base::ListValue* ctys = sig_d->FindList("ctys");
  if (ctys) {
    if (ctys->size() != bound_cert_count) {
      return std::nullopt;
    }
    for (const base::Value& cty_value : *ctys) {
      if (!cty_value.is_string()) {
        return std::nullopt;
      }
    }
  } else if (sig_d->contains("ctys")) {
    // check that there isn't a "ctys" of the wrong type
    return std::nullopt;
  }
  sig_d->Remove("ctys");

  // sigD has no other members than the aforementioned "mId", "pars", "hashM",
  // "hashV", and "ctys". (ETSI TS 119 182-1 clause 5.2.8.)
  if (!sig_d->empty()) {
    return std::nullopt;
  }
  header.Remove("sigD");

  // The header must not contain fields other than "alg", "kid", "cty",
  // "x5t#S256", "x5c", "iat", "exp", or "sigD", as required by ETSI TS 119
  // 411-5 V2.1.1, Annex B.
  //
  // ETSI TS 119 182-1 V1.2.1 section 5.1.9 specifies that if the "sigD" header
  // parameter is present, then the "crit" header parameter shall also be
  // present with "sigD" as one of its array elements. This is in conflict with
  // the requirement in 119 411-5 V2.1.1 Annex B. To resolve this conflict, this
  // implementation will allow the presence of "crit", but if it is present, it
  // must be an array containing exactly "sigD".
  const auto* crit_value = header.Find("crit");
  if (crit_value) {
    if (!crit_value->is_list()) {
      return std::nullopt;
    }
    const auto& crit_list = crit_value->GetList();
    if (crit_list.size() != 1 || !crit_list.contains("sigD")) {
      return std::nullopt;
    }
  }
  header.Remove("crit");

  if (!header.empty()) {
    return std::nullopt;
  }

  return parsed_header;
}

}  // namespace

TwoQwacCertBinding::TwoQwacCertBinding(Jades2QwacHeader header,
                                       std::string header_string,
                                       std::vector<uint8_t> signature)
    : header(header), header_string(header_string), signature(signature) {}

TwoQwacCertBinding::TwoQwacCertBinding(const TwoQwacCertBinding& other) =
    default;
TwoQwacCertBinding::TwoQwacCertBinding(TwoQwacCertBinding&& other) = default;
TwoQwacCertBinding::~TwoQwacCertBinding() = default;

std::optional<TwoQwacCertBinding> TwoQwacCertBinding::Parse(
    std::string_view jws) {
  // ETSI TS 119 411-5 V2.1.1 Annex B: The JAdES signatures shall be serialized
  // using JWS Compact Serialization as specified in IETF RFC 7515.
  //
  // The JWS Compact Serialization format consists of 3 components separated by
  // a dot (".") (RFC 7515, section 7.1).
  std::vector<std::string_view> jws_components = base::SplitStringPiece(
      jws, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jws_components.size() != 3) {
    // Reject a JWS that does not consist of 3 components.
    return std::nullopt;
  }
  std::string_view header_b64 = jws_components[0];
  std::string_view payload_b64 = jws_components[1];
  std::string_view signature_b64 = jws_components[2];

  // The 3 components of a JWS are the header, the payload, and the signature.
  // The components are base64url encoded (RFC 7515, section 7.1) and the base64
  // encoding is without any padding "=" characters (Ibid., section 2).
  std::string header_string;
  if (!base::Base64UrlDecode(header_b64,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &header_string)) {
    return std::nullopt;
  }
  std::optional<std::vector<uint8_t>> signature = base::Base64UrlDecode(
      signature_b64, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
  if (!signature.has_value()) {
    return std::nullopt;
  }

  // Parse the JWS/JAdES header.
  auto header = ParseJades2QwacHeader(header_string);
  if (!header.has_value()) {
    return std::nullopt;
  }

  // ETSI TS 119 411-5 V2.1.1 Annex B specifies a "sigD" header parameter. This
  // header parameter is defined in ETSI TS 119 182-1 V1.2.1, section 5.2.8,
  // which states "The sigD header parameter shall not appear in JAdES
  // signatures whose JWS Payload is attached". Thus, it can be inferred that
  // the JWS Payload is detached. A detached payload for a JWS means that the
  // encoded payload is empty (RFC 7515, Appendix F).
  if (!payload_b64.empty()) {
    return std::nullopt;
  }

  TwoQwacCertBinding cert_binding(*header, header_string, *signature);
  return cert_binding;
}

}  // namespace net
