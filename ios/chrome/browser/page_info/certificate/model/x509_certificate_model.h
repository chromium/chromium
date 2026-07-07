// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_H_

#include <optional>
#include <string>
#include <vector>

#include "components/certificate_model/x509_certificate_model_base.h"

namespace net {
class X509Certificate;
}

namespace x509_certificate_model {

class X509CertificateModel : public X509CertificateModelBase {
 public:
  // Construct an X509CertificateModel from a CRYPTO_BUFFER.
  explicit X509CertificateModel(bssl::UniquePtr<CRYPTO_BUFFER> cert_data);

  // Construct from net::X509Certificate (uses the leaf certificate).
  explicit X509CertificateModel(const net::X509Certificate* cert);

  X509CertificateModel(X509CertificateModel&& other);
  X509CertificateModel& operator=(X509CertificateModel&& other) = default;
  ~X509CertificateModel();

  X509CertificateModel(const X509CertificateModel&) = delete;
  X509CertificateModel& operator=(const X509CertificateModel&) = delete;

  // ---------------------------------------------------------------------------
  // These methods are always safe to call even if `cert_data` could not be
  // parsed.

  // Returns uppercase hex SHA256 hash of the certificate data (fingerprint).
  std::string HashCertSHA256() const;

  // ---------------------------------------------------------------------------
  // The rest of the methods should only be called if `is_valid()` returns true.

  // Returns uppercase hex SHA256 hash of the SPKI (fingerprint).
  std::string HashSpkiSHA256() const;

  // Get serial number as uppercase hex string with space separators between
  // bytes.
  std::string GetSerialNumberHexified() const;

  struct RDNAttribute {
    std::string oid;
    std::string label;  // e.g., "Country", "Common Name"
    std::string value;  // decoded string, or hex if it could not be decoded as
                        // a string.
  };

  // Returns every AttributeTypeAndValue in the subject / issuer DN, preserving
  // the order in which they appear in the DER encoding. This matches the
  // ordering used by Safari's certificate viewer.
  std::vector<RDNAttribute> GetSubjectAttributesInOrder() const;
  std::vector<RDNAttribute> GetIssuerAttributesInOrder() const;

  // Returns the signature algorithm name (e.g. "PKCS #1 SHA-256 With RSA
  // Encryption")
  std::string GetSignatureAlgorithm() const;

  // Returns the signature algorithm parameters if any.
  std::string GetSignatureParameters() const;

  // Returns the signature data as hex encoded string.
  std::string GetSignatureData() const;

  // Get localized public key algorithm name from the SPKI.
  std::string GetPublicKeyAlgorithm() const;

  // Get public key parameters (e.g. curve name for EC keys).
  std::string GetPublicKeyParameters() const;

  // Get public key size in bits (e.g. 2048 for RSA-2048, 256 for P-256).
  std::optional<size_t> GetPublicKeySize() const;

  // Get public key data as hex string.
  std::string GetPublicKeyData() const;
};

}  // namespace x509_certificate_model

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_H_
