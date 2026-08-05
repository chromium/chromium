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

  // The per-extension content accessors below follow a common convention: the
  // primary getter returns std::nullopt when the extension is absent or cannot
  // be decoded, and a companion Get<Ext>Raw() returns a hex dump of the
  // extension's extnValue (empty if the extension is absent) for an "Invalid"
  // fallback in the UI. Callers determine which extensions are present via
  // GetExtensionOidsInOrder() and only invoke a getter for a present extension,
  // so in practice std::nullopt means the extension failed to decode.

  // Returns true if the KeyUsage extension is present and marked critical.
  bool IsKeyUsageCritical() const;

  // Returns a comma-separated list of the asserted key usages.
  std::optional<std::string> GetKeyUsageString() const;
  std::string GetKeyUsageStringRaw() const;

  // Returns true if the ExtendedKeyUsage extension is present and marked
  // critical.
  bool IsExtendedKeyUsageCritical() const;

  // Returns the asserted EKU purposes.
  std::optional<std::vector<std::string>> GetExtendedKeyUsagePurposes() const;
  std::string GetExtendedKeyUsagePurposesRaw() const;

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

  // Returns the SubjectKeyIdentifier key identifier as a hex string.
  std::optional<std::string> GetSubjectKeyIdentifier() const;
  std::string GetSubjectKeyIdentifierRaw() const;

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

  // The following AuthorityKeyIdentifier getters are an exception to the
  // convention above: the three share one all-or-nothing parse and a single
  // GetAuthorityKeyIdentifierRaw(). If the extension fails to decode, all three
  // return std::nullopt. When the extension decodes but an individual field is
  // legitimately absent (all three are OPTIONAL), the corresponding getter
  // returns an empty (but present) value.

  // keyIdentifier as a hex string (empty if the field is absent).
  std::optional<std::string> GetAuthorityKeyIdentifier() const;

  // authorityCertIssuer decoded as GeneralNames (empty if the field is absent).
  std::optional<std::vector<GeneralName>> GetAuthorityKeyIdentifierIssuer()
      const;

  // authorityCertSerialNumber as hex (empty if the field is absent).
  std::optional<std::string> GetAuthorityKeyIdentifierSerial() const;

  std::string GetAuthorityKeyIdentifierRaw() const;

  struct AccessDescription {
    std::string method;  // localized label, e.g. "CA Issuers" or "OCSP"
    // accessLocation GeneralName, or std::nullopt if that single location
    // failed to decode. In that case `raw_location` holds a hex dump of the
    // location's bytes for the UI to show as an "Invalid General Name"
    // fallback.
    std::optional<GeneralName> location;
    // Set only when `location` is std::nullopt: a hex dump of the raw
    // accessLocation bytes.
    std::string raw_location;
  };

  // Returns true if the AuthorityInformationAccess extension is present and
  // marked critical.
  bool IsAuthorityInformationAccessCritical() const;

  // Returns the AuthorityInformationAccess descriptions in DER order; nullopt
  // only on a top-level decode failure. A single description whose
  // accessLocation fails to decode is kept (not dropped) with a nullopt
  // `location` and its bytes in `raw_location`.
  std::optional<std::vector<AccessDescription>> GetAuthorityInformationAccess()
      const;
  std::string GetAuthorityInformationAccessRaw() const;

  // Returns true if the SubjectAlternativeName extension is present and marked
  // critical.
  bool IsSubjectAlternativeNameCritical() const;

  // Returns every SubjectAlternativeName entry decoded as a GeneralName. There
  // is no Raw fallback: a SubjectAltName that fails to decode makes the whole
  // certificate invalid (is_valid() is false), so this getter is never reached
  // in that case.
  std::optional<std::vector<GeneralName>> GetSubjectAlternativeNames() const;

  // Returns true if the IssuerAlternativeName extension is present and marked
  // critical.
  bool IsIssuerAlternativeNameCritical() const;

  // Returns every IssuerAlternativeName entry decoded as a GeneralName.
  std::optional<std::vector<GeneralName>> GetIssuerAlternativeNames() const;
  std::string GetIssuerAlternativeNamesRaw() const;

  // Returns true if the CRLDistributionPoints extension is present and marked
  // critical.
  bool IsCRLDistributionPointsCritical() const;

  // Returns every CRLDistributionPoints fullName entry decoded as a
  // GeneralName, flattened across all distribution points and grouped by type
  // (URI, DNS, ...). The relativeName, reasons, and cRLIssuer fields are not
  // parsed.
  std::optional<std::vector<GeneralName>> GetCRLDistributionPointsFullNames()
      const;

  // Returns a hex dump of the CRLDistributionPoints extnValue, or empty if the
  // extension is absent.
  std::string GetCRLDistributionPointsFullNamesRaw() const;

  // A decoded RFC 5280 UserNotice policy qualifier. Each field is optional in
  // the DER encoding.
  struct UserNotice {
    // noticeRef.organization, or empty if there is no noticeRef.
    std::string organization;
    // noticeRef.noticeNumbers as decimal strings, or empty if there is no
    // noticeRef.
    std::vector<std::string> notice_numbers;
    // explicitText, or empty if absent.
    std::string explicit_text;
  };

  struct PolicyQualifier {
    enum class Type {
      kCpsUri,      // id-qt-cps: `cps_uri` holds the CPS pointer URI.
      kUserNotice,  // id-qt-unotice: `user_notice` holds the decoded notice.
      kUnknown,     // Other qualifier OID: `raw_value` holds a hex dump.
    };
    Type type;
    // Set when `type` is `kCpsUri`, empty if this is the qualifier that failed
    // to decode.
    std::string cps_uri;
    // Set when `type` is `kUserNotice`, nullopt if this is the qualifier that
    // failed to decode.
    std::optional<UserNotice> user_notice;
    // Set when `type` is `kUnknown`: hex dump of this qualifier's bytes.
    std::string raw_value;
  };

  struct CertificatePolicy {
    // The policyIdentifier.
    std::string policy_oid;
    // The policyQualifiers in DER order, ending at the failing one (if any).
    std::vector<PolicyQualifier> qualifiers;
  };

  struct CertificatePoliciesResult {
    // Decoded policies in DER order. On a per-qualifier failure, includes
    // policies up to and including the failing one. On an extension-level parse
    // failure, this is empty. Policies after a failure are omitted.
    std::vector<CertificatePolicy> policies;
    // True if decoding failed.
    bool has_error = false;
    // Hex dump of the extension's value. Populated whenever the extension is
    // present; empty when it is absent.
    std::string raw_der;
  };

  // Returns true if the CertificatePolicies extension is present and marked
  // critical.
  bool IsCertificatePoliciesCritical() const;

  // Decodes the CertificatePolicies extension. Decoding stops at the first
  // known qualifier (CPS or UserNotice) that fails to decode. See
  // `CertificatePoliciesResult` for how the result reflects success, failure,
  // and an absent extension.
  CertificatePoliciesResult GetCertificatePolicies() const;

 private:
  // Returns a hex dump of the value of the extension identified by `oid`, or an
  // empty string if the extension is absent. Used by the Get<Ext>Raw() getters
  // to surface an extension's bytes as an "Invalid" fallback.
  std::string GetExtensionRawHex(bssl::der::Input oid) const;
};

}  // namespace x509_certificate_model

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_H_
