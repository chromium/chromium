// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_CERT_BUILDER_H_
#define NET_TEST_CERT_BUILDER_H_

#include "base/rand_util.h"
#include "net/base/ip_address.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

class GURL;

namespace net {

namespace der {
class Input;
}

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
  // Initializes the CertBuilder, if |orig_cert| is non-null it will be used as
  // a template. If |issuer| is null then the generated certificate will be
  // self-signed. Otherwise, it will be signed using |issuer|.
  CertBuilder(CRYPTO_BUFFER* orig_cert, CertBuilder* issuer);
  ~CertBuilder();

  // Creates a CertBuilder that will return a static |cert| and |key|.
  // This may be passed as the |issuer| param of another CertBuilder to create
  // a cert chain that ends in a pre-defined certificate.
  static std::unique_ptr<CertBuilder> FromStaticCert(CRYPTO_BUFFER* cert,
                                                     EVP_PKEY* key);

  // Creates a simple leaf->intermediate->root chain of CertBuilders with no AIA
  // or CrlDistributionPoint extensions, and leaf having a subjectAltName of
  // www.example.com.
  static void CreateSimpleChain(std::unique_ptr<CertBuilder>* out_leaf,
                                std::unique_ptr<CertBuilder>* out_intermediate,
                                std::unique_ptr<CertBuilder>* out_root);

  // Sets a value for the indicated X.509 (v3) extension.
  void SetExtension(const der::Input& oid,
                    std::string value,
                    bool critical = false);

  // Removes an extension (if present).
  void EraseExtension(const der::Input& oid);

  // Sets the basicConstraints extension. |path_len| may be negative to
  // indicate the pathLenConstraint should be omitted.
  void SetBasicConstraints(bool is_ca, int path_len);

  // Sets an AIA extension with a single caIssuers access method.
  void SetCaIssuersUrl(const GURL& url);

  // Sets an AIA extension with the specified caIssuers and OCSP urls. Either
  // list can have 0 or more URLs, but it is an error for both lists to be
  // empty.
  void SetCaIssuersAndOCSPUrls(const std::vector<GURL>& ca_issuers_urls,
                               const std::vector<GURL>& ocsp_urls);

  // Sets a cRLDistributionPoints extension with a single DistributionPoint
  // with |url| in distributionPoint.fullName.
  void SetCrlDistributionPointUrl(const GURL& url);

  // Sets a cRLDistributionPoints extension with a single DistributionPoint
  // with |urls| in distributionPoints.fullName.
  void SetCrlDistributionPointUrls(const std::vector<GURL>& urls);

  void SetSubjectCommonName(const std::string common_name);

  // Sets the SAN for the certificate to a single dNSName.
  void SetSubjectAltName(const std::string& dns_name);

  // Sets the SAN for the certificate to the given dns names and ip addresses.
  void SetSubjectAltNames(const std::vector<std::string>& dns_names,
                          const std::vector<IPAddress>& ip_addresses);

  // Sets the extendedKeyUsage extension. |usages| should contain the DER OIDs
  // of the usage purposes to set, and must not be empty.
  void SetExtendedKeyUsages(const std::vector<der::Input>& purpose_oids);

  // Sets the certificatePolicies extension with the specified policyIdentifier
  // OIDs, which must be specified in dotted string notation (e.g. "1.2.3.4").
  void SetCertificatePolicies(const std::vector<std::string>& policy_oids);

  void SetValidity(base::Time not_before, base::Time not_after);

  // Sets the signature algorithm for the certificate to either
  // sha256WithRSAEncryption or sha1WithRSAEncryption.
  void SetSignatureAlgorithmRsaPkca1(DigestAlgorithm digest);

  void SetSignatureAlgorithm(std::string algorithm_tlv);

  void SetRandomSerialNumber();

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

  // Parses and returns validity period for the generated certificate in
  // |not_before| and |not_after|, returning true on success.
  bool GetValidity(base::Time* not_before, base::Time* not_after) const;

  // Returns the (RSA) key for the generated certificate.
  EVP_PKEY* GetKey();

  // Returns an X509Certificate for the generated certificate.
  scoped_refptr<X509Certificate> GetX509Certificate();

  // Returns an X509Certificate for the generated certificate, including
  // intermediate certificates.
  scoped_refptr<X509Certificate> GetX509CertificateChain();

  // Returns a copy of the certificate's DER.
  std::string GetDER();

 private:
  // Marks the generated certificate DER as invalid, so it will need to
  // be re-generated next time the DER is accessed.
  void Invalidate();

  // Sets the |key_| to a 2048-bit RSA key.
  void GenerateKey();

  // Generates a random subject for the certificate, comprised of just a CN.
  void GenerateSubject();

  // Parses |cert| and copies the following properties:
  //   * All extensions (dropping any duplicates)
  //   * Signature algorithm (from Certificate)
  //   * Validity (expiration)
  void InitFromCert(const der::Input& cert);

  // Assembles the CertBuilder into a TBSCertificate.
  void BuildTBSCertificate(std::string* out);

  bool AddSignatureAlgorithm(CBB* cbb);

  void GenerateCertificate();

  struct ExtensionValue {
    bool critical = false;
    std::string value;
  };

  std::string validity_tlv_;
  std::string subject_tlv_;
  std::string signature_algorithm_tlv_;
  uint64_t serial_number_ = 0;

  std::map<std::string, ExtensionValue> extensions_;

  bssl::UniquePtr<CRYPTO_BUFFER> cert_;
  bssl::UniquePtr<EVP_PKEY> key_;

  CertBuilder* issuer_ = nullptr;
};

}  // namespace net

#endif  // NET_TEST_CERT_BUILDER_H_
