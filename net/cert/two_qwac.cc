// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/two_qwac.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/types/expected.h"
#include "crypto/evp.h"
#include "crypto/signature_verifier.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"

namespace net {

Jades2QwacHeader::Jades2QwacHeader() = default;
Jades2QwacHeader::Jades2QwacHeader(const Jades2QwacHeader& other) = default;
Jades2QwacHeader::Jades2QwacHeader(Jades2QwacHeader&& other) = default;
Jades2QwacHeader::~Jades2QwacHeader() = default;

namespace {

base::expected<Jades2QwacHeader, std::string> ParseJades2QwacHeader(
    std::string_view header_string) {
  Jades2QwacHeader parsed_header;
  // The header of a JWS is a JSON-encoded object (RFC 7515, section 4).
  //
  // RFC 7515 section 5.2 (signature verification) step 3: verify the resulting
  // octet sequence (the header_string variable passed into this function) is a
  // UTF-8-encoded representation of a completely valid JSON object. By using
  // the JSONReader with base::JSON_PARSE_RFC and checking that the returned
  // value is a base::DictValue, we check that the input is UTF-8-encoded and
  // a valid JSON object.
  std::optional<base::Value> header_value =
      base::JSONReader::Read(header_string, base::JSON_PARSE_RFC);
  if (!header_value.has_value()) {
    return base::unexpected("JSON parsing error");
  }
  if (!header_value->is_dict()) {
    return base::unexpected("JSON not a dict");
  }
  // RFC 7515 section 5.2 (signature verification) step 4: If using the JWS
  // compact serialization (which we are), let the JOSE Header (the header
  // variable here) be the JWS Protected Header (the JSON object decoded in step
  // 3). During this step, verify that the resulting JOSE Header does not
  // contain duplicate Header Parameter names.
  //
  // base::JSONReader will not return an object with duplicate keys. It returns
  // the last key-value pair. This is consistent with section 4 of RFC 7515
  // which states that a JWS parser must either reject JWSs with duplicate
  // Header Parameter names or use a JSON parser that returns only the lexically
  // last duplicate member name, as specified in "The JSON Object" section of
  // the ECMAScript standard. base::JSONReader chooses this second option for
  // compliance with standards.
  base::Value::Dict& header = header_value->GetDict();

  // "alg" (Algorithm) parameter - RFC 7515, section 4.1.1
  //
  // Possible values for this field are found in the JSON Web Signature and
  // Encryption Algorithms IANA registry:
  // https://www.iana.org/assignments/jose/jose.xhtml#web-signature-encryption-algorithms
  //
  // The only requirement that the 2-QWAC spec (ETSI TS 119 411-5 Annex B)
  // imposes on this field is that it not conflict with the type of the public
  // key in the signing certificate. Annex B also states that the binding is
  // according to ETSI TS 119 182-1. Clause 5.1.2 of ETSI TS 119 182-1 merely
  // states that the syntax and semantics of this header parameter are as
  // specified in RFC 7515 section 4.1.1. In terms of allowed values, the only
  // requirement is that it shall be one specified in the aforementioned IANA
  // registry; neither ETSI TS 119 411-5 nor ETSI TS 119 182-1 specify a set of
  // required or mandatory-to-implement algorithms. The IANA registry has a
  // "JOSE Implementation Requirements" column; no (asymmetric) signature
  // algorithms are listed as "Required".
  //
  // Given that there are no required signature algorithms, this only supports
  // algorithms that at the time of writing are both listed in the IANA registry
  // and supported by crypto::SignatureVerifier.
  std::string* alg = header.FindString("alg");
  if (!alg) {
    return base::unexpected("alg missing or not a string");
  } else if (*alg == "RS256") {
    parsed_header.sig_alg = JwsSigAlg::kRsaPkcs1Sha256;
  } else if (*alg == "PS256") {
    parsed_header.sig_alg = JwsSigAlg::kRsaPssSha256;
  } else if (*alg == "ES256") {
    parsed_header.sig_alg = JwsSigAlg::kEcdsaP256Sha256;
  } else {
    return base::unexpected("unsupported alg");
  }
  header.Remove("alg");

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
  if (!cty) {
    return base::unexpected("cty missing or not a string");
  }
  if (*cty != "TLS-Certificate-Binding-v1") {
    return base::unexpected("unsupported cty");
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
    return base::unexpected("x5c missing or not a list");
  }

  size_t i = 0;
  bssl::UniquePtr<CRYPTO_BUFFER> leaf;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (const base::Value& cert_value : *x5c_list) {
    // RFC 7515 section 4.1.6:
    // "Each string in the array is a base64-encoded (not base64url-encoded) DER
    // PKIX certificate value."
    if (!cert_value.is_string()) {
      return base::unexpected("x5c element not a string");
    }
    auto cert_bytes = base::Base64Decode(cert_value.GetString());
    if (!cert_bytes.has_value()) {
      return base::unexpected("x5c element base64 decode error");
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
    return base::unexpected("x5c cert parsing error");
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
    return base::unexpected("sigD missing or not a dict");
  }

  // The sigD header must have a "mId" (mechanism ID) of
  // "http://uri.etsi.org/19182/ObjectIdByURIHash". (ETSI TS 119 411-5 Annex B.)
  std::string* m_id = sig_d->FindString("mId");
  if (!m_id) {
    return base::unexpected("sigD: mId missing or not a string");
  }
  if (*m_id != "http://uri.etsi.org/19182/ObjectIdByURIHash") {
    return base::unexpected("sigD: invalid mId");
  }
  sig_d->Remove("mId");

  // The sigD header must have a "pars" member, which is a list of strings. We
  // don't care about the contents of this list, but its size must match that of
  // "hashV". (ETSI 119 182-1 clause 5.2.8.)
  const base::ListValue* pars = sig_d->FindList("pars");
  if (!pars) {
    return base::unexpected("sigD: pars missing or not a list");
  }
  size_t bound_cert_count = pars->size();
  for (const base::Value& par : *pars) {
    if (!par.is_string()) {
      return base::unexpected("sigD: pars element not a string");
    }
  }
  sig_d->Remove("pars");

  // The sigD header must have a "hashM" member (TS 119 182-1
  // section 5.2.8.3.3), which is a string identifying the hashing algorithm
  // used for the "hashV" member. ETSI TS 119 411-5 only requires that S256,
  // S384, and S512 be supported.
  std::string* hash_m = sig_d->FindString("hashM");
  if (!hash_m) {
    return base::unexpected("sigD: hashM missing or not a string");
  }
  if (*hash_m == "S256") {
    parsed_header.hash_alg = crypto::hash::kSha256;
  } else if (*hash_m == "S384") {
    parsed_header.hash_alg = crypto::hash::kSha384;
  } else if (*hash_m == "S512") {
    parsed_header.hash_alg = crypto::hash::kSha512;
  } else {
    // Unsupported hashing algorithm.
    return base::unexpected("sigD: unsupported hashM");
  }
  sig_d->Remove("hashM");

  // The sigD header must have a "hashV" member, which is a list of
  // base64url-encoded digest values of the base64url-encoded data objects.
  // (ETSI TS 119 182-1 clause 5.2.8. The "b64" header parameter is absent, so
  // the digest is computed over the base64url-encoded data object instead of
  // computed directly over the data object.)
  const base::ListValue* hash_v = sig_d->FindList("hashV");
  if (!hash_v) {
    return base::unexpected("sigD: hashV missing or not a list");
  }
  if (hash_v->size() != bound_cert_count) {
    return base::unexpected("sigD: hashV count doesn't match pars count");
  }
  parsed_header.bound_cert_hashes.reserve(bound_cert_count);
  for (const base::Value& hash_value : *hash_v) {
    const std::string* hash_b64url = hash_value.GetIfString();
    if (!hash_b64url) {
      return base::unexpected("sigD: hashV element not a string");
    }
    // ETSI TS 119 182-1 fails to specify the definition of "base64url-encoded".
    // Given that other uses of base64url encoding come from the JWS spec, and
    // JWS disallows padding in its base64url encoding, we disallow it here as
    // well.
    auto hash = base::Base64UrlDecode(
        *hash_b64url, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
    if (!hash.has_value()) {
      return base::unexpected("sigD: hashV element base64 decode error");
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
      return base::unexpected("sigD: ctys count doesn't match pars count");
    }
    for (const base::Value& cty_value : *ctys) {
      if (!cty_value.is_string()) {
        return base::unexpected("sigD: ctys element not a string");
      }
    }
  } else if (sig_d->contains("ctys")) {
    // check that there isn't a "ctys" of the wrong type
    return base::unexpected("sigD: ctys not a list");
  }
  sig_d->Remove("ctys");

  // sigD has no other members than the aforementioned "mId", "pars", "hashM",
  // "hashV", and "ctys". (ETSI TS 119 182-1 clause 5.2.8.)
  if (!sig_d->empty()) {
    return base::unexpected("sigD has unexpected members");
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
      return base::unexpected("crit not a list");
    }
    const auto& crit_list = crit_value->GetList();
    if (crit_list.size() != 1 || !crit_list.contains("sigD")) {
      return base::unexpected("crit contains non sigD element(s)");
    }
  }
  header.Remove("crit");

  // RFC 7515 section 5.2 (signature verification) step 5: Verify that the
  // implementation understands and can process all fields that it is required
  // to support. This implementation rejects a JWS header that contains unknown
  // fields.
  if (!header.empty()) {
    return base::unexpected("header has unexpected members");
  }

  return parsed_header;
}

}  // namespace

TwoQwacCertBinding::TwoQwacCertBinding(Jades2QwacHeader header,
                                       std::string header_string,
                                       std::vector<uint8_t> signature)
    : header_(header), header_string_(header_string), signature_(signature) {}

TwoQwacCertBinding::TwoQwacCertBinding(const TwoQwacCertBinding& other) =
    default;
TwoQwacCertBinding::TwoQwacCertBinding(TwoQwacCertBinding&& other) = default;
TwoQwacCertBinding::~TwoQwacCertBinding() = default;

base::expected<TwoQwacCertBinding, std::string> TwoQwacCertBinding::Parse(
    std::string_view jws) {
  // ETSI TS 119 411-5 V2.1.1 Annex B: The JAdES signatures shall be serialized
  // using JWS Compact Serialization as specified in IETF RFC 7515.
  //
  // The JWS Compact Serialization format consists of 3 components separated by
  // a dot (".") (RFC 7515, section 7.1).
  //
  // RFC 7515 section 5.2 (signature verification) step 1: parse the JWS
  // representation to extract the serialized values for the components of the
  // JWS.
  std::vector<std::string_view> jws_components = base::SplitStringPiece(
      jws, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (jws_components.size() != 3) {
    // Reject a JWS that does not consist of 3 components.
    return base::unexpected("wrong number of components");
  }
  std::string_view header_b64 = jws_components[0];
  std::string_view payload_b64 = jws_components[1];
  std::string_view signature_b64 = jws_components[2];

  // The 3 components of a JWS are the header, the payload, and the signature.
  // The components are base64url encoded (RFC 7515, section 7.1) and the base64
  // encoding is without any padding "=" characters (Ibid., section 2).

  // RFC 7515 section 5.2 (signature verification) step 2: base64url-decode the
  // encoded representation of the JWS Protected Header.
  std::string header_string;
  if (!base::Base64UrlDecode(header_b64,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &header_string)) {
    return base::unexpected("base64 decoding header error");
  }
  // RFC 7515 section 5.2 (signature verification) step 7: base64url-decode the
  // encoded representation of the JWS Signature.
  std::optional<std::vector<uint8_t>> signature = base::Base64UrlDecode(
      signature_b64, base::Base64UrlDecodePolicy::DISALLOW_PADDING);
  if (!signature.has_value()) {
    return base::unexpected("base64 decoding signature error");
  }

  // Parse the JWS/JAdES header. This function will perform steps 3-5 of RFC
  // 7515 section 5.2 (signature verification).
  auto header = ParseJades2QwacHeader(header_string);
  if (!header.has_value()) {
    return base::unexpected("header parsing error: " + header.error());
  }

  // ETSI TS 119 411-5 V2.1.1 Annex B specifies a "sigD" header parameter. This
  // header parameter is defined in ETSI TS 119 182-1 V1.2.1, section 5.2.8,
  // which states "The sigD header parameter shall not appear in JAdES
  // signatures whose JWS Payload is attached". Thus, it can be inferred that
  // the JWS Payload is detached. A detached payload for a JWS means that the
  // encoded payload is empty (RFC 7515, Appendix F).
  //
  // RFC 7515 section 5.2 (signature verification) step 6: base64url-decode the
  // encoded representation of the JWS Payload. Since the only valid payload is
  // the empty payload, checking that the encoded representation is empty is
  // sufficient to decode and check that the JWS Payload is empty.
  if (!payload_b64.empty()) {
    return base::unexpected("payload is non-empty");
  }

  return TwoQwacCertBinding(*header, std::string(header_b64), *signature);
}

namespace {

// Given a SPKI, returns whether the public key is an ECDSA key on the curve
// P-256.
bool IsKeyP256(base::span<const uint8_t> spki) {
  bssl::UniquePtr<EVP_PKEY> public_key = crypto::evp::PublicKeyFromBytes(spki);
  if (!public_key) {
    return false;
  }
  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(public_key.get());
  if (!ec_key) {
    return false;
  }
  const EC_GROUP* group = EC_KEY_get0_group(ec_key);
  if (!group) {
    return false;
  }
  return EC_GROUP_get_curve_name(group) == NID_X9_62_prime256v1;
}

}  // namespace

bool TwoQwacCertBinding::VerifySignature() {
  // ETSI TS 119 411-5 clause 6.2.2 step 5 states:
  //
  //   Validate the JAdES signature on the TLS Certificate binding according to
  //   ETSI EN 319 102-1.
  //
  //     - If this step fails or the TLS Certificate binding is not considered
  //       valid, the procedure finishes negatively.
  //
  // ETSI EN 319 102-1 does not say how to validate a JAdES signature. If we
  // attempt to apply the processes that it describes generically for AdES
  // signatures, we encounter a problem in the cryptographic validation building
  // block in clause 5.2.7.4. That clause states that the technical details on
  // how to perform the cryptographic validation are out of scope, and to see
  // other documents for details. None of the listed documents provide any
  // details about JAdES signatures or JWSs.
  //
  // Since ETSI EN 319 102-1 lacks a pointer to the proper specification
  // containing the technical details needed to cryptographically validate a
  // JAdES signature, I look at the 2-QWAC spec (ETSI TS 119 411-5) which cited
  // ETSI EN 319 102-1 for assistance. ETSI TS 119 411-5 includes ETSI TS 119
  // 182-1 ("JAdES digital signatures") as a normative reference. ETSI TS 119
  // 182-1 clause 1 defines the scope of that document, and the validation of
  // JAdES digital signatures is out of scope for that document. Although the
  // validation of JAdES digital signatures is out of scope for that document,
  // it does define a JAdES signature as being an extension of JSON Web
  // Signatures as specified in IETF RFC 7515.
  //
  // For lack of a better reference, this 2-QWAC implementation will use the
  // process defined in section 5.2 of RFC 7515 (Message Signature or MAC
  // Validation) to validate the signature on the TLS Certificate Binding JWS/
  // JAdES signature. This function only implements the process defined in RFC
  // 7515; it does not implement any of the other building blocks used by the
  // validation process for Basic Signatures defined in clause 5.3 of ETSI EN
  // 319 102-1.

  // Extract public key from certificate and initialize verifier. ETSI TS 119
  // 411-5 Annex B requires checking that the "alg" parameter does not conflict
  // with the type of public key in the signing certificate. The call to
  // VerifyInit checks that the signature algorithm is compatible with the
  // signing key (from the signing certificate).
  std::string_view spki;
  if (!asn1::ExtractSPKIFromDERCert(x509_util::CryptoBufferAsStringPiece(
                                        header_.two_qwac_cert->cert_buffer()),
                                    &spki)) {
    return false;
  }
  crypto::SignatureVerifier::SignatureAlgorithm sig_alg;
  switch (header_.sig_alg) {
    case JwsSigAlg::kEcdsaP256Sha256:
      // SignatureAlgorithm::ECDSA_SHA256 doesn't require that the EC curve be
      // P-256, but the JWS signature algorithm does require that it be P-256.
      // Before converting JwsSigAlg::kEcdsaP256Sha256 to ECDSA_SHA256, check
      // that the key is P-256.
      if (!IsKeyP256(base::as_byte_span(spki))) {
        return false;
      }
      sig_alg = crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
      break;
    case JwsSigAlg::kRsaPkcs1Sha256:
      sig_alg = crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
      break;
    case JwsSigAlg::kRsaPssSha256:
      sig_alg = crypto::SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256;
      break;
  }
  // The crypto::SignatureVerifier checks that the public key in |spki| is
  // compatible with the signature algorithm in |sig_alg| that came from the JWS
  // header. This handles the requirement in the 2-QWAC spec (ETSI TS 119 411-5
  // Annex B) that the "alg" JWS header field not conflict with the type of the
  // public key in the "x5c" JWS header field.
  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(sig_alg, signature_, base::as_byte_span(spki))) {
    return false;
  }

  // RFC 7515 section 5.2 steps 1-7 are performed by TwoQwacCertBinding::Parse.

  // Step 8: Validate the JWS Signature against the JWS Signing Input.
  //
  // The JWS Signing Input is ASCII(BASE64URL(UTF8(JWS Protected Header)) || '.'
  // || BASE64URL(JWS Payload)) (RFC 7515 section 5.2 step 8).
  //
  // The first component of the input - BASE64URL(UTF8(JWS Protected Header)) -
  // is the unparsed JWS header:
  verifier.VerifyUpdate(base::as_byte_span(header_string_));
  static constexpr uint8_t separator[] = {'.'};
  verifier.VerifyUpdate(separator);
  // The JWS Payload is empty, so there are 0 bytes to contribute to the
  // BASE64URL(JWS Payload) component of the JWS Signing Input.

  // Step 9 only applies if the JWS JSON Serialization is being used; we use the
  // JWS Compact Serialization.

  // Step 10: In the JWS Compact Serialization case, the result can simply
  // indicate whether or not the JWS was successfully validated.
  return verifier.VerifyFinal();
}

bool TwoQwacCertBinding::BindsTlsCert(base::span<const uint8_t> tls_cert_der) {
  // header.bound_cert_hashes contains a list of Digest(base64url(der)), where
  // the digest algorithm is specified by header.hash_alg. Compute the digest of
  // the base64url-encoded cert and search for that in the list of bound cert
  // hashes.
  std::string tls_cert_b64;
  base::Base64UrlEncode(tls_cert_der, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &tls_cert_b64);
  std::vector<uint8_t> tls_cert_hash(
      crypto::hash::DigestSizeForHashKind(header_.hash_alg));
  crypto::hash::Hash(header_.hash_alg, tls_cert_b64, tls_cert_hash);

  return base::Contains(header_.bound_cert_hashes, tls_cert_hash);
}

}  // namespace net
