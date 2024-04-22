// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/revocation_builder.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "net/cert/asn1_util.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/pki/input.h"

namespace net {

namespace {

std::string Sha1() {
  // SEQUENCE { OBJECT_IDENTIFIER { 1.3.14.3.2.26 } }
  const uint8_t kSHA1[] = {0x30, 0x07, 0x06, 0x05, 0x2b,
                           0x0e, 0x03, 0x02, 0x1a};
  return std::string(std::begin(kSHA1), std::end(kSHA1));
}

// Adds bytes (specified as a std::string_view) to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
bool CBBAddBytes(CBB* cbb, std::string_view bytes) {
  return CBB_add_bytes(cbb, reinterpret_cast<const uint8_t*>(bytes.data()),
                       bytes.size());
}

// Adds bytes (specified as a span) to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
bool CBBAddBytes(CBB* cbb, base::span<const uint8_t> data) {
  return CBB_add_bytes(cbb, data.data(), data.size());
}

// Adds a GeneralizedTime value to the given CBB.
// The argument ordering follows the boringssl CBB_* api style.
bool CBBAddGeneralizedTime(CBB* cbb, const base::Time& time) {
  bssl::der::GeneralizedTime generalized_time;
  if (!EncodeTimeAsGeneralizedTime(time, &generalized_time)) {
    return false;
  }
  CBB time_cbb;
  uint8_t out[bssl::der::kGeneralizedTimeLength];
  if (!bssl::der::EncodeGeneralizedTime(generalized_time, out) ||
      !CBB_add_asn1(cbb, &time_cbb, CBS_ASN1_GENERALIZEDTIME) ||
      !CBBAddBytes(&time_cbb, out) || !CBB_flush(cbb)) {
    return false;
  }
  return true;
}

// Finalizes the CBB to a std::string.
std::string FinishCBB(CBB* cbb) {
  size_t cbb_len;
  uint8_t* cbb_bytes;

  if (!CBB_finish(cbb, &cbb_bytes, &cbb_len)) {
    ADD_FAILURE() << "CBB_finish() failed";
    return std::string();
  }

  bssl::UniquePtr<uint8_t> delete_bytes(cbb_bytes);
  return std::string(reinterpret_cast<char*>(cbb_bytes), cbb_len);
}

std::string PKeyToSPK(const EVP_PKEY* pkey) {
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 64) || !EVP_marshal_public_key(cbb.get(), pkey)) {
    ADD_FAILURE();
    return std::string();
  }
  std::string spki = FinishCBB(cbb.get());

  std::string_view spk;
  if (!asn1::ExtractSubjectPublicKeyFromSPKI(spki, &spk)) {
    ADD_FAILURE();
    return std::string();
  }

  // ExtractSubjectPublicKeyFromSPKI() includes the unused bit count. For this
  // application, the unused bit count must be zero, and is not included in the
  // result.
  if (spk.empty() || spk[0] != '\0') {
    ADD_FAILURE();
    return std::string();
  }
  spk.remove_prefix(1);

  return std::string(spk);
}

// Returns a DER-encoded bssl::OCSPResponse with the given |response_status|.
// |response_type| and |response| are optional and may be empty.
std::string EncodeOCSPResponse(
    bssl::OCSPResponse::ResponseStatus response_status,
    bssl::der::Input response_type,
    std::string response) {
  // RFC 6960 section 4.2.1:
  //
  //    bssl::OCSPResponse ::= SEQUENCE {
  //       responseStatus         OCSPResponseStatus,
  //       responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL }
  //
  //    OCSPResponseStatus ::= ENUMERATED {
  //        successful            (0),  -- Response has valid confirmations
  //        malformedRequest      (1),  -- Illegal confirmation request
  //        internalError         (2),  -- Internal error in issuer
  //        tryLater              (3),  -- Try again later
  //                                    -- (4) is not used
  //        sigRequired           (5),  -- Must sign the request
  //        unauthorized          (6)   -- Request unauthorized
  //    }
  //
  //    The value for responseBytes consists of an OBJECT IDENTIFIER and a
  //    response syntax identified by that OID encoded as an OCTET STRING.
  //
  //    ResponseBytes ::=       SEQUENCE {
  //        responseType   OBJECT IDENTIFIER,
  //        response       OCTET STRING }
  bssl::ScopedCBB cbb;
  CBB ocsp_response, ocsp_response_status, ocsp_response_bytes,
      ocsp_response_bytes_sequence, ocsp_response_type,
      ocsp_response_octet_string;

  if (!CBB_init(cbb.get(), 64 + response_type.size() + response.size()) ||
      !CBB_add_asn1(cbb.get(), &ocsp_response, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&ocsp_response, &ocsp_response_status,
                    CBS_ASN1_ENUMERATED) ||
      !CBB_add_u8(&ocsp_response_status,
                  static_cast<uint8_t>(response_status))) {
    ADD_FAILURE();
    return std::string();
  }

  if (!response_type.empty()) {
    if (!CBB_add_asn1(&ocsp_response, &ocsp_response_bytes,
                      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0) ||
        !CBB_add_asn1(&ocsp_response_bytes, &ocsp_response_bytes_sequence,
                      CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1(&ocsp_response_bytes_sequence, &ocsp_response_type,
                      CBS_ASN1_OBJECT) ||
        !CBBAddBytes(&ocsp_response_type, response_type) ||
        !CBB_add_asn1(&ocsp_response_bytes_sequence,
                      &ocsp_response_octet_string, CBS_ASN1_OCTETSTRING) ||
        !CBBAddBytes(&ocsp_response_octet_string, response)) {
      ADD_FAILURE();
      return std::string();
    }
  }

  return FinishCBB(cbb.get());
}

// Adds a DER-encoded OCSP SingleResponse to |responses_cbb|.
// |issuer_name_hash| and |issuer_key_hash| should be binary SHA1 hashes.
bool AddOCSPSingleResponse(CBB* responses_cbb,
                           const OCSPBuilderSingleResponse& response,
                           const std::string& issuer_name_hash,
                           const std::string& issuer_key_hash) {
  // RFC 6960 section 4.2.1:
  //
  //    SingleResponse ::= SEQUENCE {
  //       certID                       CertID,
  //       certStatus                   CertStatus,
  //       thisUpdate                   GeneralizedTime,
  //       nextUpdate         [0]       EXPLICIT GeneralizedTime OPTIONAL,
  //       singleExtensions   [1]       EXPLICIT Extensions OPTIONAL }
  //
  //    CertStatus ::= CHOICE {
  //        good        [0]     IMPLICIT NULL,
  //        revoked     [1]     IMPLICIT RevokedInfo,
  //        unknown     [2]     IMPLICIT UnknownInfo }
  //
  //    RevokedInfo ::= SEQUENCE {
  //        revocationTime              GeneralizedTime,
  //        revocationReason    [0]     EXPLICIT CRLReason OPTIONAL }
  //
  //    UnknownInfo ::= NULL
  //
  // RFC 6960 section 4.1.1:
  //   CertID          ::=     SEQUENCE {
  //        hashAlgorithm       AlgorithmIdentifier,
  //        issuerNameHash      OCTET STRING, -- Hash of issuer's DN
  //        issuerKeyHash       OCTET STRING, -- Hash of issuer's public key
  //        serialNumber        CertificateSerialNumber }
  //
  //  The contents of CertID include the following fields:
  //
  //    o  hashAlgorithm is the hash algorithm used to generate the
  //       issuerNameHash and issuerKeyHash values.
  //
  //    o  issuerNameHash is the hash of the issuer's distinguished name
  //       (DN).  The hash shall be calculated over the DER encoding of the
  //       issuer's name field in the certificate being checked.
  //
  //    o  issuerKeyHash is the hash of the issuer's public key.  The hash
  //       shall be calculated over the value (excluding tag and length) of
  //       the subject public key field in the issuer's certificate.
  //
  //    o  serialNumber is the serial number of the certificate for which
  //       status is being requested.

  CBB single_response, issuer_name_hash_cbb, issuer_key_hash_cbb, cert_id;
  if (!CBB_add_asn1(responses_cbb, &single_response, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&single_response, &cert_id, CBS_ASN1_SEQUENCE) ||
      !CBBAddBytes(&cert_id, Sha1()) ||
      !CBB_add_asn1(&cert_id, &issuer_name_hash_cbb, CBS_ASN1_OCTETSTRING) ||
      !CBBAddBytes(&issuer_name_hash_cbb, issuer_name_hash) ||
      !CBB_add_asn1(&cert_id, &issuer_key_hash_cbb, CBS_ASN1_OCTETSTRING) ||
      !CBBAddBytes(&issuer_key_hash_cbb, issuer_key_hash) ||
      !CBB_add_asn1_uint64(&cert_id, response.serial)) {
    ADD_FAILURE();
    return false;
  }

  unsigned int cert_status_tag_number;
  switch (response.cert_status) {
    case bssl::OCSPRevocationStatus::GOOD:
      cert_status_tag_number = CBS_ASN1_CONTEXT_SPECIFIC | 0;
      break;
    case bssl::OCSPRevocationStatus::REVOKED:
      cert_status_tag_number =
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1;
      break;
    case bssl::OCSPRevocationStatus::UNKNOWN:
      cert_status_tag_number = CBS_ASN1_CONTEXT_SPECIFIC | 2;
      break;
  }

  CBB cert_status_cbb;
  if (!CBB_add_asn1(&single_response, &cert_status_cbb,
                    cert_status_tag_number)) {
    ADD_FAILURE();
    return false;
  }
  if (response.cert_status == bssl::OCSPRevocationStatus::REVOKED &&
      !CBBAddGeneralizedTime(&cert_status_cbb, response.revocation_time)) {
    ADD_FAILURE();
    return false;
  }

  CBB next_update_cbb;
  if (!CBBAddGeneralizedTime(&single_response, response.this_update) ||
      !CBB_add_asn1(&single_response, &next_update_cbb,
                    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0) ||
      !CBBAddGeneralizedTime(&next_update_cbb, response.next_update)) {
    ADD_FAILURE();
    return false;
  }

  return CBB_flush(responses_cbb);
}

}  // namespace

std::string BuildOCSPResponseError(
    bssl::OCSPResponse::ResponseStatus response_status) {
  DCHECK_NE(response_status, bssl::OCSPResponse::ResponseStatus::SUCCESSFUL);
  return EncodeOCSPResponse(response_status, bssl::der::Input(), std::string());
}

std::string BuildOCSPResponse(
    const std::string& responder_subject,
    EVP_PKEY* responder_key,
    base::Time produced_at,
    const std::vector<OCSPBuilderSingleResponse>& responses) {
  std::string responder_name_hash = base::SHA1HashString(responder_subject);
  std::string responder_key_hash =
      base::SHA1HashString(PKeyToSPK(responder_key));

  // RFC 6960 section 4.2.1:
  //
  //    ResponseData ::= SEQUENCE {
  //       version              [0] EXPLICIT Version DEFAULT v1,
  //       responderID              ResponderID,
  //       producedAt               GeneralizedTime,
  //       responses                SEQUENCE OF SingleResponse,
  //       responseExtensions   [1] EXPLICIT Extensions OPTIONAL }
  //
  //    ResponderID ::= CHOICE {
  //       byName               [1] Name,
  //       byKey                [2] KeyHash }
  //
  //    KeyHash ::= OCTET STRING -- SHA-1 hash of responder's public key
  //    (excluding the tag and length fields)
  bssl::ScopedCBB tbs_cbb;
  CBB response_data, responder_id, responder_id_by_key, responses_cbb;
  if (!CBB_init(tbs_cbb.get(), 64) ||
      !CBB_add_asn1(tbs_cbb.get(), &response_data, CBS_ASN1_SEQUENCE) ||
      // Version is the default v1, so it is not encoded.
      !CBB_add_asn1(&response_data, &responder_id,
                    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 2) ||
      !CBB_add_asn1(&responder_id, &responder_id_by_key,
                    CBS_ASN1_OCTETSTRING) ||
      !CBBAddBytes(&responder_id_by_key, responder_key_hash) ||
      !CBBAddGeneralizedTime(&response_data, produced_at) ||
      !CBB_add_asn1(&response_data, &responses_cbb, CBS_ASN1_SEQUENCE)) {
    ADD_FAILURE();
    return std::string();
  }

  for (const auto& response : responses) {
    if (!AddOCSPSingleResponse(&responses_cbb, response, responder_name_hash,
                               responder_key_hash)) {
      return std::string();
    }
  }

  // responseExtensions not currently supported.

  return BuildOCSPResponseWithResponseData(responder_key,
                                           FinishCBB(tbs_cbb.get()));
}

std::string BuildOCSPResponseWithResponseData(
    EVP_PKEY* responder_key,
    const std::string& tbs_response_data,
    std::optional<bssl::SignatureAlgorithm> signature_algorithm) {
  //    For a basic OCSP responder, responseType will be id-pkix-ocsp-basic.
  //
  //    id-pkix-ocsp           OBJECT IDENTIFIER ::= { id-ad-ocsp }
  //    id-pkix-ocsp-basic     OBJECT IDENTIFIER ::= { id-pkix-ocsp 1 }
  //
  //    The value for response SHALL be the DER encoding of
  //    BasicOCSPResponse.
  //
  //    BasicOCSPResponse       ::= SEQUENCE {
  //       tbsResponseData      ResponseData,
  //       signatureAlgorithm   AlgorithmIdentifier,
  //       signature            BIT STRING,
  //       certs            [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
  //
  //    The value for signature SHALL be computed on the hash of the DER
  //    encoding of ResponseData.  The responder MAY include certificates in
  //    the certs field of BasicOCSPResponse that help the OCSP client verify
  //    the responder's signature.  If no certificates are included, then
  //    certs SHOULD be absent.
  //
  bssl::ScopedCBB basic_ocsp_response_cbb;
  CBB basic_ocsp_response, signature;
  if (!responder_key) {
    ADD_FAILURE();
    return std::string();
  }
  if (!signature_algorithm)
    signature_algorithm =
        CertBuilder::DefaultSignatureAlgorithmForKey(responder_key);
  if (!signature_algorithm) {
    ADD_FAILURE();
    return std::string();
  }
  std::string signature_algorithm_tlv =
      CertBuilder::SignatureAlgorithmToDer(*signature_algorithm);
  if (signature_algorithm_tlv.empty() ||
      !CBB_init(basic_ocsp_response_cbb.get(), 64 + tbs_response_data.size()) ||
      !CBB_add_asn1(basic_ocsp_response_cbb.get(), &basic_ocsp_response,
                    CBS_ASN1_SEQUENCE) ||
      !CBBAddBytes(&basic_ocsp_response, tbs_response_data) ||
      !CBBAddBytes(&basic_ocsp_response, signature_algorithm_tlv) ||
      !CBB_add_asn1(&basic_ocsp_response, &signature, CBS_ASN1_BITSTRING) ||
      !CBB_add_u8(&signature, 0 /* no unused bits */) ||
      !CertBuilder::SignData(*signature_algorithm, tbs_response_data,
                             responder_key, &signature)) {
    ADD_FAILURE();
    return std::string();
  }

  // certs field not currently supported.

  return EncodeOCSPResponse(bssl::OCSPResponse::ResponseStatus::SUCCESSFUL,
                            bssl::der::Input(bssl::kBasicOCSPResponseOid),
                            FinishCBB(basic_ocsp_response_cbb.get()));
}

std::string BuildCrlWithSigner(
    const std::string& crl_issuer_subject,
    EVP_PKEY* crl_issuer_key,
    const std::vector<uint64_t>& revoked_serials,
    const std::string& signature_algorithm_tlv,
    base::OnceCallback<bool(std::string, CBB*)> signer) {
  if (!crl_issuer_key) {
    ADD_FAILURE();
    return std::string();
  }
  //    TBSCertList  ::=  SEQUENCE  {
  //         version                 Version OPTIONAL,
  //                                      -- if present, MUST be v2
  //         signature               AlgorithmIdentifier,
  //         issuer                  Name,
  //         thisUpdate              Time,
  //         nextUpdate              Time OPTIONAL,
  //         revokedCertificates     SEQUENCE OF SEQUENCE  {
  //              userCertificate         CertificateSerialNumber,
  //              revocationDate          Time,
  //              crlEntryExtensions      Extensions OPTIONAL
  //                                       -- if present, version MUST be v2
  //                                   }  OPTIONAL,
  //         crlExtensions           [0]  EXPLICIT Extensions OPTIONAL
  //                                       -- if present, version MUST be v2
  //                                   }
  bssl::ScopedCBB tbs_cbb;
  CBB tbs_cert_list, revoked_serials_cbb;
  if (!CBB_init(tbs_cbb.get(), 10) ||
      !CBB_add_asn1(tbs_cbb.get(), &tbs_cert_list, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1_uint64(&tbs_cert_list, 1 /* V2 */) ||
      !CBBAddBytes(&tbs_cert_list, signature_algorithm_tlv) ||
      !CBBAddBytes(&tbs_cert_list, crl_issuer_subject) ||
      !x509_util::CBBAddTime(&tbs_cert_list,
                             base::Time::Now() - base::Days(1)) ||
      !x509_util::CBBAddTime(&tbs_cert_list,
                             base::Time::Now() + base::Days(6))) {
    ADD_FAILURE();
    return std::string();
  }
  if (!revoked_serials.empty()) {
    if (!CBB_add_asn1(&tbs_cert_list, &revoked_serials_cbb,
                      CBS_ASN1_SEQUENCE)) {
      ADD_FAILURE();
      return std::string();
    }
    for (const int64_t revoked_serial : revoked_serials) {
      CBB revoked_serial_cbb;
      if (!CBB_add_asn1(&revoked_serials_cbb, &revoked_serial_cbb,
                        CBS_ASN1_SEQUENCE) ||
          !CBB_add_asn1_uint64(&revoked_serial_cbb, revoked_serial) ||
          !x509_util::CBBAddTime(&revoked_serial_cbb,
                                 base::Time::Now() - base::Days(1)) ||
          !CBB_flush(&revoked_serials_cbb)) {
        ADD_FAILURE();
        return std::string();
      }
    }
  }

  std::string tbs_tlv = FinishCBB(tbs_cbb.get());

  //    CertificateList  ::=  SEQUENCE  {
  //         tbsCertList          TBSCertList,
  //         signatureAlgorithm   AlgorithmIdentifier,
  //         signatureValue       BIT STRING  }
  bssl::ScopedCBB crl_cbb;
  CBB cert_list, signature;
  if (!CBB_init(crl_cbb.get(), 10) ||
      !CBB_add_asn1(crl_cbb.get(), &cert_list, CBS_ASN1_SEQUENCE) ||
      !CBBAddBytes(&cert_list, tbs_tlv) ||
      !CBBAddBytes(&cert_list, signature_algorithm_tlv) ||
      !CBB_add_asn1(&cert_list, &signature, CBS_ASN1_BITSTRING) ||
      !CBB_add_u8(&signature, 0 /* no unused bits */) ||
      !std::move(signer).Run(tbs_tlv, &signature)) {
    ADD_FAILURE();
    return std::string();
  }
  return FinishCBB(crl_cbb.get());
}

std::string BuildCrl(
    const std::string& crl_issuer_subject,
    EVP_PKEY* crl_issuer_key,
    const std::vector<uint64_t>& revoked_serials,
    std::optional<bssl::SignatureAlgorithm> signature_algorithm) {
  if (!signature_algorithm) {
    signature_algorithm =
        CertBuilder::DefaultSignatureAlgorithmForKey(crl_issuer_key);
  }
  if (!signature_algorithm) {
    ADD_FAILURE();
    return std::string();
  }
  std::string signature_algorithm_tlv =
      CertBuilder::SignatureAlgorithmToDer(*signature_algorithm);
  if (signature_algorithm_tlv.empty()) {
    ADD_FAILURE();
    return std::string();
  }

  auto signer =
      base::BindLambdaForTesting([&](std::string tbs_tlv, CBB* signature) {
        return CertBuilder::SignData(*signature_algorithm, tbs_tlv,
                                     crl_issuer_key, signature);
      });
  return BuildCrlWithSigner(crl_issuer_subject, crl_issuer_key, revoked_serials,
                            signature_algorithm_tlv, signer);
}

std::string BuildCrlWithAlgorithmTlvAndDigest(
    const std::string& crl_issuer_subject,
    EVP_PKEY* crl_issuer_key,
    const std::vector<uint64_t>& revoked_serials,
    const std::string& signature_algorithm_tlv,
    const EVP_MD* digest) {
  auto signer =
      base::BindLambdaForTesting([&](std::string tbs_tlv, CBB* signature) {
        return CertBuilder::SignDataWithDigest(digest, tbs_tlv, crl_issuer_key,
                                               signature);
      });
  return BuildCrlWithSigner(crl_issuer_subject, crl_issuer_key, revoked_serials,
                            signature_algorithm_tlv, signer);
}

}  // namespace net
