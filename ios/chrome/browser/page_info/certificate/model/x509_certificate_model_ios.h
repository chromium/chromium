// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_IOS_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_IOS_H_

#import <map>
#import <optional>
#import <string>
#import <string_view>
#import <vector>

#import "components/certificate_model/x509_certificate_model_base.h"

namespace net {
class X509Certificate;
}

namespace bssl {
struct GeneralNames;
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

  // Returns upper case hex SHA256 hash of the certificate data (fingerprint).
  std::string HashCertSHA256() const;

  // ---------------------------------------------------------------------------
  // The rest of the methods should only be called if `is_valid()` returns true.

  // Returns upper case hex SHA256 hash of the SPKI.
  std::string HashSpkiSHA256() const;

  int GetVersion() const;

  // Get serial number as hex string.
  std::string GetSerialNumberHexified() const;

  // Get subject name fields beyond those provided by the base class.
  OptionalStringOrError GetSubjectCountryName() const;
  OptionalStringOrError GetSubjectLocalityName() const;
  OptionalStringOrError GetSubjectStateName() const;
  OptionalStringOrError GetSubjectBusinessCategory() const;
  OptionalStringOrError GetSubjectJurisdictionCountryName() const;
  OptionalStringOrError GetSubjectJurisdictionStateName() const;
  OptionalStringOrError GetSubjectSerialNumber() const;

  // Get issuer name fields beyond those provided by the base class.
  OptionalStringOrError GetIssuerCountryName() const;

  // Get public key size in bits (e.g., 2048 for RSA-2048, 256 for P-256).
  std::optional<size_t> GetPublicKeySize() const;

  // Get localized public key algorithm name from the SPKI.
  std::string GetPublicKeyAlgorithm() const;

  // Get public key data as hex string.
  std::string GetPublicKeyData() const;

  // BasicConstraints extension information.
  bool HasBasicConstraints() const;
  bool IsBasicConstraintsCritical() const;
  bool IsBasicConstraintsCa() const;
  std::optional<uint8_t> GetBasicConstraintsPathLen() const;

  // KeyUsage extension information.
  bool HasKeyUsage() const;
  bool IsKeyUsageCritical() const;
  std::string GetKeyUsageString() const;

  // ExtendedKeyUsage extension information.
  bool HasExtendedKeyUsage() const;
  bool IsExtendedKeyUsageCritical() const;
  std::vector<std::string> GetExtendedKeyUsagePurposes() const;

  // SubjectKeyIdentifier extension information.
  bool HasSubjectKeyIdentifier() const;
  bool IsSubjectKeyIdentifierCritical() const;
  std::string GetSubjectKeyIdentifier() const;

  // Signature information.
  std::string GetSignatureAlgorithm() const;
  std::string GetSignatureParameters() const;
  std::string GetSignatureData() const;

  // AuthorityKeyIdentifier extension information.
  bool HasAuthorityKeyIdentifier() const;
  bool IsAuthorityKeyIdentifierCritical() const;
  std::string GetAuthorityKeyIdentifier() const;

  // AuthorityInformationAccess extension information.
  struct AccessDescription {
    std::string method;    // e.g., "OCSP", "CA Issuers"
    std::string location;  // URI
  };
  bool HasAuthorityInformationAccess() const;
  bool IsAuthorityInformationAccessCritical() const;
  std::vector<AccessDescription> GetAuthorityInformationAccess() const;

  // CRLDistributionPoints extension information.
  bool HasCRLDistributionPoints() const;
  bool IsCRLDistributionPointsCritical() const;
  std::vector<std::string> GetCRLDistributionPoints() const;

  // CertificatePolicies extension information.
  struct PolicyQualifier {
    PolicyQualifier();
    ~PolicyQualifier();
    PolicyQualifier(const PolicyQualifier&);
    PolicyQualifier& operator=(const PolicyQualifier&);

    std::string oid;
    std::string description;
    std::string type_name;
    std::string value;
  };

  struct CertificatePolicy {
    CertificatePolicy();
    ~CertificatePolicy();
    CertificatePolicy(const CertificatePolicy&);
    CertificatePolicy& operator=(const CertificatePolicy&);

    std::string identifier;
    std::vector<PolicyQualifier> qualifiers;
  };

  bool HasCertificatePolicies() const;
  bool IsCertificatePoliciesCritical() const;
  std::vector<CertificatePolicy> GetCertificatePolicies() const;

  // SubjectAlternativeNames extension information.
  bool HasSubjectAlternativeNames() const;
  bool IsSubjectAlternativeNamesCritical() const;
  std::vector<std::string> GetSubjectAlternativeNamesDNS() const;

  // Signed Certificate Timestamp List extension information.
  bool HasSCTList() const;
  bool IsSCTListCritical() const;
  std::string GetSCTListData() const;

  // Unknown/Other extensions information.
  struct UnknownExtension {
    std::string oid;
    bool critical;
    std::string raw_data;
  };
  std::vector<UnknownExtension> GetUnknownExtensions() const;

 private:
  bool ParseExtensions(const bssl::der::Input& extensions_tlv);

  std::map<bssl::der::Input, bssl::ParsedExtension> extensions_;

  // Parsed SubjectAltName extension.
  std::unique_ptr<bssl::GeneralNames> subject_alt_names_;
};

}  // namespace x509_certificate_model

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_IOS_H_
