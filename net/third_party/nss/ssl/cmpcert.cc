/*
 * NSS utility functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "net/third_party/nss/ssl/cmpcert.h"

#include <secder.h>
#include <secitem.h>

#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parser.h"

namespace net {

namespace {

bool GetIssuerAndSubject(CERTCertificate* cert,
                         bssl::der::Input* issuer,
                         bssl::der::Input* subject) {
  *issuer = bssl::der::Input(cert->derIssuer.data, cert->derIssuer.len);
  *subject = bssl::der::Input(cert->derSubject.data, cert->derSubject.len);
  return true;
}

bool GetIssuerAndSubject(X509Certificate* cert,
                         bssl::der::Input* issuer,
                         bssl::der::Input* subject) {
  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;
  if (!bssl::ParseCertificate(
          bssl::der::Input(CRYPTO_BUFFER_data(cert->cert_buffer()),
                           CRYPTO_BUFFER_len(cert->cert_buffer())),
          &tbs_certificate_tlv, &signature_algorithm_tlv, &signature_value,
          nullptr)) {
    return false;
  }
  bssl::ParsedTbsCertificate tbs;
  if (!ParseTbsCertificate(tbs_certificate_tlv,
                           x509_util::DefaultParseCertificateOptions(), &tbs,
                           nullptr)) {
    return false;
  }

  *issuer = tbs.issuer_tlv;
  *subject = tbs.subject_tlv;
  return true;
}

}  // namespace

bool MatchClientCertificateIssuers(
    X509Certificate* cert,
    const std::vector<std::string>& cert_authorities,
    ScopedCERTCertificateList* intermediates) {
  // Bound how many iterations to try.
  static const int kMaxDepth = 20;

  intermediates->clear();

  // If no authorities are supplied, everything matches.
  if (cert_authorities.empty())
    return true;

  // DER encoded issuer and subject name of current certificate.
  bssl::der::Input issuer;
  bssl::der::Input subject;

  if (!GetIssuerAndSubject(cert, &issuer, &subject))
    return false;

  while (intermediates->size() < kMaxDepth) {
    // Check if current cert is issued by a valid CA.
    for (const std::string& ca : cert_authorities) {
      if (issuer == bssl::der::Input(ca)) {
        return true;
      }
    }

    // Stop at self-issued certificates.
    if (issuer == subject)
      return false;

    // Look the parent up in the database and keep searching.
    SECItem issuer_item;
    issuer_item.len = issuer.size();
    issuer_item.data = const_cast<unsigned char*>(issuer.data());
    ScopedCERTCertificate nextcert(
        CERT_FindCertByName(CERT_GetDefaultCertDB(), &issuer_item));
    if (!nextcert)
      return false;

    if (!GetIssuerAndSubject(nextcert.get(), &issuer, &subject))
      return false;

    intermediates->push_back(std::move(nextcert));
  }

  return false;
}

}  // namespace net
