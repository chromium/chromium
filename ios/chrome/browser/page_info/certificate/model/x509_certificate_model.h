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

  // Returns the OIDs of every extension in the order they appear in the
  // certificate's DER encoding.
  std::vector<bssl::der::Input> GetExtensionOidsInOrder() const;

  // Returns true if the KeyUsage extension is present and marked critical.
  bool IsKeyUsageCritical() const;

  // Returns a comma-separated list of the asserted key usages, or an empty
  // string if the extension is not present or could not be parsed.
  std::string GetKeyUsageString() const;

  // Returns true if the ExtendedKeyUsage extension is present and marked
  // critical.
  bool IsExtendedKeyUsageCritical() const;

  // Returns a vector of the asserted EKU purposes, or an empty vector if the
  // extension is not present or could not be parsed.
  std::vector<std::string> GetExtendedKeyUsagePurposes() const;

  // Returns true if the BasicConstraints extension is present and marked
  // critical.
  bool IsBasicConstraintsCritical() const;

  // Returns true if the BasicConstraints extension is present and asserts the
  // cA boolean.
  bool IsBasicConstraintsCA() const;

  // Returns the BasicConstraints pathLenConstraint, or nullopt if the extension
  // is absent, could not be parsed, or does not specify a path length.
  std::optional<uint8_t> GetBasicConstraintsPathLen() const;

  // Returns true if the SubjectKeyIdentifier extension is present and marked
  // critical.
  bool IsSubjectKeyIdentifierCritical() const;

  // Returns the SubjectKeyIdentifier key identifier as a hex string, or an
  // empty string if the extension is not present or could not be parsed.
  std::string GetSubjectKeyIdentifier() const;

  // A single decoded RFC 5280 GeneralName referenced from bssl::GeneralNames.
  // `value` holds the decoded text for textual variants
  // (URI/DNS/email/IP/registeredID) or hex for opaque ones
  // (x400Address/ediPartyName).
  struct GeneralName {
    enum class Type {
      kOtherName,
      kRFC822Name,
      kDNSName,
      kX400Address,
      kDirectoryName,
      kEDIPartyName,
      kURI,
      kIPAddress,
      kRegisteredID,
    };
    Type type;
    std::string value;
    // For `kOtherName` only: the type-id. Empty for all other types.
    std::string other_name_oid;
    // For `kDirectoryName` only: the parsed RDNs. Empty for all other types.
    std::vector<RDNAttribute> directory_name;
  };

  // Returns true if the AuthorityKeyIdentifier extension is present and marked
  // critical.
  bool IsAuthorityKeyIdentifierCritical() const;

  // Returns the AuthorityKeyIdentifier keyIdentifier as a hex string, or an
  // empty string if the extension is not present, could not be parsed, or does
  // not contain a keyIdentifier.
  std::string GetAuthorityKeyIdentifier() const;

  // Returns the authorityCertIssuer entries decoded as GeneralNames, grouped by
  // type (directoryName, URI, DNS, ...). Empty if the extension is absent or
  // does not contain an authorityCertIssuer.
  std::vector<GeneralName> GetAuthorityKeyIdentifierIssuer() const;

  // Returns the authorityCertSerialNumber as hex, or an empty string if absent.
  std::string GetAuthorityKeyIdentifierSerial() const;

  struct AccessDescription {
    std::string method;    // localized label, e.g. "CA Issuers" or "OCSP"
    GeneralName location;  // accessLocation GeneralName
  };

  // Returns true if the AuthorityInformationAccess extension is present and
  // marked critical.
  bool IsAuthorityInformationAccessCritical() const;

  // Returns the AuthorityInformationAccess descriptions in DER order, or an
  // empty vector if the extension is absent or could not be parsed.
  std::vector<AccessDescription> GetAuthorityInformationAccess() const;

  // Returns true if the SubjectAlternativeName extension is present and marked
  // critical.
  bool IsSubjectAlternativeNameCritical() const;

  // Returns every SubjectAlternativeName entry decoded as a GeneralName,
  // grouped by type (DNS, IP, URI, ...). Empty if the extension is absent or
  // cannot be parsed.
  std::vector<GeneralName> GetSubjectAlternativeNames() const;

  // Returns true if the IssuerAlternativeName extension is present and marked
  // critical.
  bool IsIssuerAlternativeNameCritical() const;

  // Returns every IssuerAlternativeName entry decoded as a GeneralName, grouped
  // by type (DNS, IP, URI, ...). Empty if the extension is absent or cannot be
  // parsed.
  std::vector<GeneralName> GetIssuerAlternativeNames() const;

  // Returns true if the CRLDistributionPoints extension is present and marked
  // critical.
  bool IsCRLDistributionPointsCritical() const;

  // Returns every CRLDistributionPoints fullName entry decoded as a
  // GeneralName, flattened across all distribution points and grouped by type
  // (URI, DNS, ...). The relativeName, reasons, and cRLIssuer fields are not
  // parsed. Empty if the extension is absent or cannot be parsed.
  std::vector<GeneralName> GetCRLDistributionPointsFullNames() const;
};

}  // namespace x509_certificate_model

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_H_
