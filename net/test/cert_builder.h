// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_CERT_BUILDER_H_
#define NET_TEST_CERT_BUILDER_H_

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "net/base/hash_value.h"
#include "net/base/ip_address.h"
#include "net/cert/qwac.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/merkle_tree.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "third_party/boringssl/src/pki/trust_store.h"

class GURL;

namespace base {
class FilePath;
}

namespace bssl {
namespace der {
class Input;
}  // namespace der
}  // namespace bssl

namespace net {

// CertBuilder is a helper class to dynamically create a test certificate.
//
// CertBuilder is initialized using an existing certificate, from which it
// copies most properties (see InitFromCert for details).
//
// The subject, serial number, and key for the final certificate are chosen
// randomly. Using a randomized subject and serial number is important to defeat
// certificate caching done by NSS, which otherwise can make test outcomes
// dependent on ordering.
class CertBuilder {
 public:
  // Parameters for creating an embedded SignedCertificateTimestamp.
  struct SctConfig {
    SctConfig();
    SctConfig(std::string log_id,
              bssl::UniquePtr<EVP_PKEY> log_key,
              base::Time timestamp);
    SctConfig(const SctConfig&);
    SctConfig(SctConfig&&);
    ~SctConfig();
    SctConfig& operator=(const SctConfig&);
    SctConfig& operator=(SctConfig&&);

    std::string log_id;
    // Only EC keys are supported currently.
    bssl::UniquePtr<EVP_PKEY> log_key;
    base::Time timestamp;
  };

  // Initializes the CertBuilder, if |orig_cert| is non-null it will be used as
  // a template. If |issuer| is null then the generated certificate will be
  // self-signed. Otherwise, it will be signed using |issuer|.
  CertBuilder(CRYPTO_BUFFER* orig_cert, CertBuilder* issuer);
  ~CertBuilder();

  // Initializes a CertBuilder using the certificate and private key from
  // |cert_and_key_file| as a template. If |issuer| is null then the generated
  // certificate will be self-signed. Otherwise, it will be signed using
  // |issuer|.
  static std::unique_ptr<CertBuilder> FromFile(
      const base::FilePath& cert_and_key_file,
      CertBuilder* issuer);

  // Initializes a CertBuilder that will return a certificate for the provided
  // public key |spki_der|. It will be signed with the |issuer|, this builder
  // will not have a private key, so it cannot produce self-signed certificates
  // and |issuer| cannot be null.
  static std::unique_ptr<CertBuilder> FromSubjectPublicKeyInfo(
      base::span<const uint8_t> spki_der,
      CertBuilder* issuer);

  // Creates a CertBuilder that will return a static |cert| and |key|.
  // This may be passed as the |issuer| param of another CertBuilder to create
  // a cert chain that ends in a pre-defined certificate.
  static std::unique_ptr<CertBuilder> FromStaticCert(CRYPTO_BUFFER* cert,
                                                     EVP_PKEY* key);
  // Like FromStaticCert, but loads the certificate and private key from the
  // PEM file |cert_and_key_file|.
  static std::unique_ptr<CertBuilder> FromStaticCertFile(
      const base::FilePath& cert_and_key_file);

  // Creates a simple chain of CertBuilders with no AIA or CrlDistributionPoint
  // extensions, and leaf having a subjectAltName of www.example.com.
  // The chain is returned in leaf-first order.
  static std::vector<std::unique_ptr<CertBuilder>> CreateSimpleChain(
      size_t chain_length);

  // Creates a simple leaf->intermediate->root chain of CertBuilders with no AIA
  // or CrlDistributionPoint extensions, and leaf having a subjectAltName of
  // www.example.com.
  static std::array<std::unique_ptr<CertBuilder>, 3> CreateSimpleChain3();

  // Creates a simple leaf->root chain of CertBuilders with no AIA or
  // CrlDistributionPoint extensions, and leaf having a subjectAltName of
  // www.example.com.
  static std::array<std::unique_ptr<CertBuilder>, 2> CreateSimpleChain2();

  // Returns a compatible signature algorithm for |key|.
  static std::optional<bssl::SignatureAlgorithm>
  DefaultSignatureAlgorithmForKey(EVP_PKEY* key);

  // Signs |tbs_data| with |key| using |signature_algorithm| appending the
  // signature onto |out_signature| and returns true if successful.
  static bool SignData(bssl::SignatureAlgorithm signature_algorithm,
                       std::string_view tbs_data,
                       EVP_PKEY* key,
                       CBB* out_signature);

  static bool SignDataWithDigest(const EVP_MD* digest,
                                 std::string_view tbs_data,
                                 EVP_PKEY* key,
                                 CBB* out_signature);

  // Returns a DER encoded AlgorithmIdentifier TLV for |signature_algorithm|
  // empty string on error.
  static std::string SignatureAlgorithmToDer(
      bssl::SignatureAlgorithm signature_algorithm);

  // Generates |num_bytes| random bytes, and then returns the hex encoding of
  // those bytes.
  static std::string MakeRandomHexString(size_t num_bytes);

  // Builds a DER encoded X.501 Name TLV containing a commonName of
  // |common_name| with type |common_name_tag|.
  static std::vector<uint8_t> BuildNameWithCommonNameOfType(
      std::string_view common_name,
      unsigned common_name_tag);

  // Returns a DER encoded SEQUENCE OF OBJECT IDENTIFIER from the vector of
  // OID values.
  static std::vector<uint8_t> BuildSequenceOfOid(
      std::vector<bssl::der::Input> oids);

  // Set the version of the certificate. Note that only V3 certificates may
  // contain extensions, so if |version| is |V1| or |V2| you may want to also
  // call |ClearExtensions()| unless you intentionally want to generate an
  // invalid certificate.
  void SetCertificateVersion(bssl::CertificateVersion version);

  // Sets a value for the indicated X.509 (v3) extension.
  void SetExtension(const bssl::der::Input& oid,
                    std::string value,
                    bool critical = false);

  // Removes an extension (if present).
  void EraseExtension(const bssl::der::Input& oid);

  // Removes all extensions.
  void ClearExtensions();

  // Sets the basicConstraints extension. |path_len| may be negative to
  // indicate the pathLenConstraint should be omitted.
  void SetBasicConstraints(bool is_ca, int path_len);

  // Sets the nameConstraints extension. |permitted_dns_names| lists permitted
  // dnsName subtrees. |excluded_dns_names| lists excluded dnsName subtrees. If
  // both lists are empty the extension is removed.
  void SetNameConstraintsDnsNames(
      const std::vector<std::string>& permitted_dns_names,
      const std::vector<std::string>& excluded_dns_names);

  // Sets an AIA extension with a single caIssuers access method.
  void SetCaIssuersUrl(const GURL& url);

  // Sets an AIA extension with the specified caIssuers and OCSP urls. Either
  // list can have 0 or more URLs. If both are empty, the AIA extension is
  // removed.
  void SetCaIssuersAndOCSPUrls(const std::vector<GURL>& ca_issuers_urls,
                               const std::vector<GURL>& ocsp_urls);
  // Same as |SetCaIssuersAndOCSPUrls| above, but the inputs can be arbitrary
  // strings.
  void SetCaIssuersAndOCSPUrls(const std::vector<std::string>& ca_issuers_urls,
                               const std::vector<std::string>& ocsp_urls);

  // Sets a cRLDistributionPoints extension with a single DistributionPoint
  // with |url| in distributionPoint.fullName.
  void SetCrlDistributionPointUrl(const GURL& url);
  // Same as |SetCrlDistributionPointUrl| above, but the inputs can be an
  // arbitrary string.
  void SetCrlDistributionPointUrl(const std::string_view& url);

  // Sets a cRLDistributionPoints extension with a single DistributionPoint
  // with |urls| in distributionPoints.fullName.
  void SetCrlDistributionPointUrls(const std::vector<GURL>& urls);
  // Same as |SetCrlDistributionPointUrls| above, but the inputs can be
  // arbitrary strings.
  void SetCrlDistributionPointUrls(const std::vector<std::string>& urls);

  // Sets the issuer bytes that will be encoded into the generated certificate.
  // If this is not called, or |issuer_tlv| is empty, the subject field from
  // the issuer CertBuilder will be used.
  void SetIssuerTLV(base::span<const uint8_t> issuer_tlv);

  // Sets the subject to a Name with a single commonName attribute with
  // the value |common_name| tagged as a UTF8String.
  void SetSubjectCommonName(std::string_view common_name);

  // Sets the subject to |subject_tlv|.
  void SetSubjectTLV(base::span<const uint8_t> subject_tlv);

  // Sets the SAN for the certificate to a single dNSName.
  void SetSubjectAltName(std::string_view dns_name);

  // Sets the SAN for the certificate to the given dns names and ip addresses.
  void SetSubjectAltNames(const std::vector<std::string>& dns_names,
                          const std::vector<IPAddress>& ip_addresses);

  // Sets the keyUsage extension. |usages| should contain the bssl::KeyUsageBit
  // values of the usages to set, and must not be empty.
  void SetKeyUsages(const std::vector<bssl::KeyUsageBit>& usages);

  // Sets the extendedKeyUsage extension. |usages| should contain the DER OIDs
  // of the usage purposes to set, and must not be empty.
  void SetExtendedKeyUsages(const std::vector<bssl::der::Input>& purpose_oids);

  // Sets the certificatePolicies extension with the specified policyIdentifier
  // OIDs, which must be specified in dotted string notation (e.g. "1.2.3.4").
  // If |policy_oids| is empty, the extension will be removed.
  void SetCertificatePolicies(const std::vector<std::string>& policy_oids);

  // Sets the policyMappings extension with the specified mappings, which are
  // pairs of issuerDomainPolicy -> subjectDomainPolicy mappings in dotted
  // string notation.
  // If |policy_mappings| is empty, the extension will be removed.
  void SetPolicyMappings(
      const std::vector<std::pair<std::string, std::string>>& policy_mappings);

  // Sets the PolicyConstraints extension. If both |require_explicit_policy|
  // and |inhibit_policy_mapping| are nullopt, the PolicyConstraints extension
  // will removed.
  void SetPolicyConstraints(std::optional<uint64_t> require_explicit_policy,
                            std::optional<uint64_t> inhibit_policy_mapping);

  // Sets the inhibitAnyPolicy extension.
  void SetInhibitAnyPolicy(uint64_t skip_certs);

  // Sets the QcStatements extension with statements as specified by
  // `qc_statements`.
  void SetQcStatements(std::vector<QcStatement> qc_statements);

  // Sets the QcStatements extension to have QWAC statements: a QcCompliance
  // statement with no info and a QcType statement with the info being the OIDs
  // with values from `qc_types`.
  void SetQwacQcStatements(std::vector<bssl::der::Input> qc_types);

  void SetValidity(base::Time not_before, base::Time not_after);

  // Sets the Subject Key Identifier (SKI) extension to the specified string.
  // By default, a unique SKI will be generated for each CertBuilder; however,
  // this may be overridden to force multiple certificates to be considered
  // during path building on systems that prioritize matching SKI to the
  // Authority Key Identifier (AKI) extension, rather than using the
  // Subject/Issuer name. Empty SKIs are not supported; use EraseExtension()
  // for that.
  void SetSubjectKeyIdentifier(const std::string& subject_key_identifier);

  // Sets the Authority Key Identifier (AKI) extension to the specified
  // string.
  // Note: Only the keyIdentifier option is supported, and the value
  // is the raw identifier (i.e. without DER encoding). Empty strings will
  // result in the extension, if present, being erased. This ensures that it
  // is safe to use SetAuthorityKeyIdentifier() with the result of the
  // issuing CertBuilder's (if any) GetSubjectKeyIdentifier() without
  // introducing AKI/SKI chain building issues.
  void SetAuthorityKeyIdentifier(const std::string& authority_key_identifier);

  // Sets the signature algorithm to use in generating the certificate's
  // signature. The signature algorithm should be compatible with
  // the type of |issuer_->GetKey()|. If this method is not called, and the
  // CertBuilder was initialized from a template cert, the signature algorithm
  // of that cert will be used, or if there was no template cert, a default
  // algorithm will be used base on the signing key type.
  void SetSignatureAlgorithm(bssl::SignatureAlgorithm signature_algorithm);

  // Sets both signature AlgorithmIdentifier TLVs to encode in the generated
  // certificate.
  // This only affects the bytes written to the output - it does not affect what
  // algorithm is actually used to perform the signature. To set the signature
  // algorithm used to generate the certificate's signature, use
  // |SetSignatureAlgorithm|. If this method is not called, the signature
  // algorithm written to the output will be chosen to match the signature
  // algorithm used to sign the certificate.
  void SetSignatureAlgorithmTLV(std::string_view signature_algorithm_tlv);

  // Set only the outer Certificate signatureAlgorithm TLV. See
  // SetSignatureAlgorithmTLV comment for general notes.
  void SetOuterSignatureAlgorithmTLV(std::string_view signature_algorithm_tlv);

  // Set only the tbsCertificate signature TLV. See SetSignatureAlgorithmTLV
  // comment for general notes.
  void SetTBSSignatureAlgorithmTLV(std::string_view signature_algorithm_tlv);

  void SetSerialNumber(uint64_t serial_number);
  void SetRandomSerialNumber();

  // Sets the configuration that will be used to generate a
  // SignedCertificateTimestampList extension in the certificate.
  void SetSctConfig(std::vector<CertBuilder::SctConfig> sct_configs);

  // Sets the private key for the generated certificate to an EC key. If a key
  // was already set, it will be replaced.
  void GenerateECKey();

  // Sets the private key for the generated certificate to a 2048-bit RSA key.
  // RSA key generation is expensive, so this should not be used unless an RSA
  // key is specifically needed. If a key was already set, it will be replaced.
  void GenerateRSAKey();

  // Loads the private key for the generated certificate from |key_file|.
  bool UseKeyFromFile(const base::FilePath& key_file);

  // Sets the private key to be |key|.
  void SetKey(bssl::UniquePtr<EVP_PKEY> key);

  // Returns the CertBuilder that issues this certificate. (Will be |this| if
  // certificate is self-signed.)
  CertBuilder* issuer() { return issuer_; }

  // Returns a CRYPTO_BUFFER to the generated certificate.
  CRYPTO_BUFFER* GetCertBuffer();

  bssl::UniquePtr<CRYPTO_BUFFER> DupCertBuffer();

  // Returns the subject of the generated certificate.
  const std::string& GetSubject();

  // Returns the serial number for the generated certificate.
  uint64_t GetSerialNumber();

  // Returns the subject key identifier for the generated certificate. If
  // none is present, a random value will be generated.
  // Note: The returned value will be the contents of the OCTET
  // STRING/KeyIdentifier, without DER encoding, ensuring it's suitable for
  // SetSubjectKeyIdentifier().
  std::string GetSubjectKeyIdentifier();

  // Parses and returns validity period for the generated certificate in
  // |not_before| and |not_after|, returning true on success.
  bool GetValidity(base::Time* not_before, base::Time* not_after) const;

  // Get the DER-encoded validity.
  base::span<const uint8_t> GetEncodedValidity() {
    return base::as_byte_span(validity_tlv_);
  }

  // Get the DER-encoded extensions, or an empty vector if there are no
  // extensions.
  void GetEncodedExtensions(std::vector<uint8_t>* out);

  // Returns the key for the generated certificate.
  EVP_PKEY* GetKey();

  // Returns an X509Certificate for the generated certificate.
  scoped_refptr<X509Certificate> GetX509Certificate();

  // Returns an X509Certificate for the generated certificate, including
  // intermediate certificates (not including the self-signed root).
  scoped_refptr<X509Certificate> GetX509CertificateChain();

  // Returns an X509Certificate for the generated certificate, including
  // intermediate certificates and the self-signed root.
  scoped_refptr<X509Certificate> GetX509CertificateFullChain();

  // Returns a copy of the certificate's DER.
  std::string GetDER();

  // Returns a copy of the certificate as PEM encoded DER.
  // Convenience method for debugging, to more easily log what cert is being
  // created.
  std::string GetPEM();

  // Returns the full chain (including root) as PEM.
  // Convenience method for debugging, to more easily log what certs are being
  // created.
  std::string GetPEMFullChain();

  // Returns the private key as PEM.
  // Convenience method for debugging, to more easily log what certs are being
  // created.
  std::string GetPrivateKeyPEM();

 private:
  // Initializes the CertBuilder, if |orig_cert| is non-null it will be used as
  // a template. If |issuer| is null then the generated certificate will be
  // self-signed. Otherwise, it will be signed using |issuer|.
  // |unique_subject_key_identifier| controls whether an ephemeral SKI will
  // be generated for this certificate. In general, any manipulation of the
  // certificate at all should result in a new SKI, to avoid issues on
  // Windows CryptoAPI, but generating a unique SKI can create issues for
  // macOS Security.framework if |orig_cert| has already issued certificates
  // (including self-signed certs). The only time this is safe is thus
  // when used in conjunction with FromStaticCert() and re-using the
  // same key, thus this constructor is private.
  CertBuilder(CRYPTO_BUFFER* orig_cert,
              CertBuilder* issuer,
              bool unique_subject_key_identifier);

  // Marks the generated certificate DER as invalid, so it will need to
  // be re-generated next time the DER is accessed.
  void Invalidate();

  // Generates a random Subject Key Identifier for the certificate. This is
  // necessary for Windows, which otherwises uses SKI/AKI matching for lookups
  // with greater precedence than subject/issuer name matching, and on newer
  // versions of Windows, limits the number of lookups+signature failures that
  // can be performed. Rather than deriving from |key_|, generating a unique
  // value is useful for signalling this is a "unique" and otherwise
  // independent CA.
  void GenerateSubjectKeyIdentifier();

  // Generates a random subject for the certificate, comprised of just a CN.
  void GenerateSubject();

  // Parses |cert| and copies the following properties:
  //   * All extensions (dropping any duplicates)
  //   * Signature algorithm (from Certificate)
  //   * Validity (expiration)
  void InitFromCert(const bssl::der::Input& cert);

  // Assembles the CertBuilder into a TBSCertificate.
  void BuildTBSCertificate(std::string_view signature_algorithm_tlv,
                           std::string* out);

  void BuildSctListExtension(const std::string& pre_tbs_certificate,
                             std::string* out);

  void GenerateCertificate();

  void SetCaIssuersAndOCSPUrls(
      const std::vector<std::pair<bssl::der::Input, std::string_view>>&
          entries);

  struct ExtensionValue {
    bool critical = false;
    std::string value;
  };

  bssl::CertificateVersion version_ = bssl::CertificateVersion::V3;
  std::string validity_tlv_;
  std::optional<std::string> issuer_tlv_;
  std::string subject_tlv_;
  std::optional<bssl::SignatureAlgorithm> signature_algorithm_;
  std::string outer_signature_algorithm_tlv_;
  std::string tbs_signature_algorithm_tlv_;
  uint64_t serial_number_ = 0;
  int default_pkey_id_ = EVP_PKEY_EC;

  std::vector<SctConfig> sct_configs_;

  std::map<std::string, ExtensionValue> extensions_;

  bssl::UniquePtr<CRYPTO_BUFFER> cert_;
  bssl::UniquePtr<EVP_PKEY> key_;

  raw_ptr<CertBuilder, DanglingUntriaged> issuer_ = nullptr;
};

class MtcLogBuilder {
 public:
  // Type aliases to make interfaces more obvious what the integer types mean.
  using LandmarkNumber = uint64_t;
  using LogIndex = uint64_t;

  explicit MtcLogBuilder(base::span<const uint8_t> log_id);
  ~MtcLogBuilder();

  // Creates the next landmark. Returns false on failure (eg if there were
  // no new entries added since the last landmark).
  bool AdvanceLandmark();

  // Returns the range, inclusive, of active landmark numbers.
  //
  // Active landmarks are those that may contain un-expired certificates
  // (https://davidben.github.io/merkle-tree-certs/draft-davidben-tls-merkle-tree-certs.html#section-6.3.1-7)
  //
  // This implementation doesn't directly care about expiration or validity
  // periods and leaves those details to the test to control. It does not
  // currently support advancing the minimum landmark, but that could be added
  // if a test needs it.
  //
  // Landmark numbers can be used to form Trust Anchor IDs
  // https://davidben.github.io/merkle-tree-certs/draft-davidben-tls-merkle-tree-certs.html#section-6.3.1-4
  std::pair<LandmarkNumber, LandmarkNumber> GetActiveLandmarkRange() {
    return {0, landmarks_.size() - 1};
  }

  // Returns the currently active landmark subtrees.
  //
  // https://davidben.github.io/merkle-tree-certs/draft-davidben-tls-merkle-tree-certs.html#section-6.3.1-7
  std::vector<bssl::Subtree> GetLandmarkSubtrees();

  // Returns the subtrees and subtree hashes for the currently active
  // landmarks. This information is needed by the client to verify
  // signatureless certificates.
  //
  // https://davidben.github.io/merkle-tree-certs/draft-davidben-tls-merkle-tree-certs.html#trusted-subtrees
  std::vector<bssl::TrustedSubtree> GetLandmarkSubtreeHashes();

  // Add entry to the log and return the index of the entry.
  // Once the index is included in a landmark subtree, the index can be used
  // with CreateSignaturelessCertificate to create a certificate.
  // TODO(crbug.com/469624806): using CertBuilder for this is slightly odd,
  // refactor so that MTC certs have a builder that only contains methods
  // that are relevant for MTCs (sharing code with the legacy CertBuilder in
  // whatever way makes sense).
  LogIndex AddEntry(CertBuilder& mtc_builder);

  // Add entries to the log that will not actually be used. This can be used
  // to make the merkle tree in a certain shape without having to create a
  // bunch of otherwise unused MTC cert builders.
  // `extra_data` will be hashed into the entries, and can be used to test
  // logs with the same shape trees with different merkle tree hashes.
  void AddUnusedEntries(size_t n, base::span<const uint8_t> extra_data = {});

  // Returns the DER-encoded certificate for entry `index`, which must be
  // included in the active landmark subtrees. Returns nullopt otherwise.
  //
  // https://davidben.github.io/merkle-tree-certs/draft-davidben-tls-merkle-tree-certs.html#name-constructing-signatureless-
  std::optional<std::vector<uint8_t>> CreateSignaturelessCertificate(
      LogIndex index);

  // Like CreateSignaturelessCertificate, but returns a CRYPTO_BUFFER instead
  // of a byte vector, or returns nullptr on error.
  bssl::UniquePtr<CRYPTO_BUFFER> CreateSignaturelessCertificateBuffer(
      LogIndex index);

 private:
  class Data;

  std::vector<uint8_t> GetEncodedLogName();
  std::vector<uint8_t> CreateSignaturelessMtcProof(LogIndex index);
  std::vector<SHA256HashValue> CalculateSubtreeInclusionProof(
      bssl::Subtree subtree,
      bssl::Subtree tree);

  // TBSCertificateLogEntry  ::=  SEQUENCE  {
  // version             [0]  EXPLICIT Version DEFAULT v1,
  // issuer                   Name,
  // validity                 Validity,
  // subject                  Name,
  // subjectPublicKeyInfoHash OCTET STRING,
  // issuerUniqueID      [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  // subjectUniqueID     [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  // extensions          [3]  EXPLICIT Extensions OPTIONAL }
  struct MtcLogEntry {
    MtcLogEntry();
    ~MtcLogEntry();
    MtcLogEntry(const MtcLogEntry&);
    MtcLogEntry& operator=(const MtcLogEntry& other);
    MtcLogEntry(MtcLogEntry&&);
    MtcLogEntry& operator=(MtcLogEntry&& other);

    static MtcLogEntry NullEntry();

    std::vector<uint8_t> BuildMerkleTreeCertEntryTbsCertEntry(
        std::vector<uint8_t> issuer_tlv);
    std::vector<uint8_t> BuildTBSCertificate(std::vector<uint8_t> issuer_tlv,
                                             uint64_t index);

    // Fields corresponding to TBSCertificateLogEntry:
    // TODO(crbug.com/469624806): Version is always v3. Support
    // CertBuilder::version_?
    // Issuer is not present since it is always derived from the builder's
    // `log_id_`.
    std::vector<uint8_t> validity;
    std::vector<uint8_t> subject;
    // subjectPublicKeyInfoHash isn't saved in the struct, it is calculated
    // from the `subject_public_key_info` when needed.
    // issuerUniqueID and subjectUniqueID are not supported.
    std::vector<uint8_t> extensions;

    // Additional fields for creating a final certificate:
    std::vector<uint8_t> subject_public_key_info;
  };

  // The tree size at each landmark (the vector is a mapping from
  // LandmarkNumber to LogIndex). Landmark 0 is always the empty tree.
  std::vector<LogIndex> landmarks_;

  std::vector<uint8_t> log_id_;

  std::unique_ptr<Data> data_;
};

}  // namespace net

#endif  // NET_TEST_CERT_BUILDER_H_
