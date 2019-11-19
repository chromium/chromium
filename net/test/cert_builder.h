// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_CERT_BUILDER_H_
#define NET_TEST_CERT_BUILDER_H_

#include "base/rand_util.h"
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
  // Initializes the CertBuilder using |orig_cert|. If |issuer| is null
  // then the generated certificate will be self-signed. Otherwise, it
  // will be signed using |issuer|.
  CertBuilder(CRYPTO_BUFFER* orig_cert, CertBuilder* issuer);
  ~CertBuilder();

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

  // Sets the certificatePolicies extension with the specified policyIdentifier
  // OIDs, which must be specified in dotted string notation (e.g. "1.2.3.4").
  void SetCertificatePolicies(const std::vector<std::string>& policy_oids);

  void SetValidity(base::Time not_before, base::Time not_after);

  // Sets the signature algorithm for the certificate to either
  // sha256WithRSAEncryption or sha1WithRSAEncryption.
  void SetSignatureAlgorithmRsaPkca1(DigestAlgorithm digest);

  void SetSignatureAlgorithm(std::string algorithm_tlv);

  void SetRandomSerialNumber();

  // Returns a CRYPTO_BUFFER to the generated certificate.
  CRYPTO_BUFFER* GetCertBuffer();

  bssl::UniquePtr<CRYPTO_BUFFER> DupCertBuffer();

  // Returns the subject of the generated certificate.
  const std::string& GetSubject();

  // Returns the serial number for the generated certificate.
  uint64_t GetSerialNumber();

  // Returns the (RSA) key for the generated certificate.
  EVP_PKEY* GetKey();

  // Returns an X509Certificate for the generated certificate.
  scoped_refptr<X509Certificate> GetX509Certificate();

  // Returns an X509Certificate for the generated certificate, including
  // intermediate certificates.
  scoped_refptr<X509Certificate> GetX509CertificateChain();

  // Returns a copy of the certificate's DER.
  std::string GetDER();

  // Creates a CRL issued and signed by |crl_issuer|, marking |revoked_serials|
  // as revoked.
  // Returns the DER-encoded CRL.
  static std::string CreateCrl(CertBuilder* crl_issuer,
                               const std::vector<uint64_t>& revoked_serials,
                               DigestAlgorithm digest);

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
