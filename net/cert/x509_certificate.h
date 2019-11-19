// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_CERTIFICATE_H_
#define NET_CERT_X509_CERTIFICATE_H_

#include <stddef.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/x509_cert_types.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace base {
class Pickle;
class PickleIterator;
}

namespace net {

class X509Certificate;

typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;

// X509Certificate represents a X.509 certificate, which is comprised a
// particular identity or end-entity certificate, such as an SSL server
// identity or an SSL client certificate, and zero or more intermediate
// certificates that may be used to build a path to a root certificate.
class NET_EXPORT X509Certificate
    : public base::RefCountedThreadSafe<X509Certificate> {
 public:
  enum PublicKeyType {
    kPublicKeyTypeUnknown,
    kPublicKeyTypeRSA,
    kPublicKeyTypeDSA,
    kPublicKeyTypeECDSA,
    kPublicKeyTypeDH,
    kPublicKeyTypeECDH
  };

  enum Format {
    // The data contains a single DER-encoded certificate, or a PEM-encoded
    // DER certificate with the PEM encoding block name of "CERTIFICATE".
    // Any subsequent blocks will be ignored.
    FORMAT_SINGLE_CERTIFICATE = 1 << 0,

    // The data contains a sequence of one or more PEM-encoded, DER
    // certificates, with the PEM encoding block name of "CERTIFICATE".
    // All PEM blocks will be parsed, until the first error is encountered.
    FORMAT_PEM_CERT_SEQUENCE = 1 << 1,

    // The data contains a PKCS#7 SignedData structure, whose certificates
    // member is to be used to initialize the certificate and intermediates.
    // The data may further be encoded using PEM, specifying block names of
    // either "PKCS7" or "CERTIFICATE".
    FORMAT_PKCS7 = 1 << 2,

    // Automatically detect the format.
    FORMAT_AUTO = FORMAT_SINGLE_CERTIFICATE | FORMAT_PEM_CERT_SEQUENCE |
                  FORMAT_PKCS7,
  };

  // Create an X509Certificate from a CRYPTO_BUFFER containing the DER-encoded
  // representation. Returns NULL on failure to parse or extract data from the
  // the certificate. Note that this does not guarantee the certificate is
  // fully parsed and validated, only that the members of this class, such as
  // subject, issuer, expiry times, and serial number, could be successfully
  // initialized from the certificate.
  static scoped_refptr<X509Certificate> CreateFromBuffer(
      bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer,
      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates);

  // Options for configuring certificate parsing.
  // Do not use without consulting //net owners.
  struct UnsafeCreateOptions {
    bool printable_string_is_utf8 = false;
  };
  // Create an X509Certificate with non-standard parsing options.
  // Do not use without consulting //net owners.
  static scoped_refptr<X509Certificate> CreateFromBufferUnsafeOptions(
      bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer,
      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates,
      UnsafeCreateOptions options);

  // Create an X509Certificate from a chain of DER encoded certificates. The
  // first certificate in the chain is the end-entity certificate to which a
  // handle is returned. The other certificates in the chain are intermediate
  // certificates.
  static scoped_refptr<X509Certificate> CreateFromDERCertChain(
      const std::vector<base::StringPiece>& der_certs);

  // Create an X509Certificate from a chain of DER encoded certificates with
  // non-standard parsing options.
  // Do not use without consulting //net owners.
  static scoped_refptr<X509Certificate> CreateFromDERCertChainUnsafeOptions(
      const std::vector<base::StringPiece>& der_certs,
      UnsafeCreateOptions options);

  // Create an X509Certificate from the DER-encoded representation.
  // Returns NULL on failure.
  static scoped_refptr<X509Certificate> CreateFromBytes(const char* data,
                                                        size_t length);

  // Create an X509Certificate with non-standard parsing options.
  // Do not use without consulting //net owners.
  static scoped_refptr<X509Certificate> CreateFromBytesUnsafeOptions(
      const char* data,
      size_t length,
      UnsafeCreateOptions options);

  // Create an X509Certificate from the representation stored in the given
  // pickle.  The data for this object is found relative to the given
  // pickle_iter, which should be passed to the pickle's various Read* methods.
  // Returns NULL on failure.
  static scoped_refptr<X509Certificate> CreateFromPickle(
      base::PickleIterator* pickle_iter);

  // Create an X509Certificate from the representation stored in the given
  // pickle with non-standard parsing options.
  // Do not use without consulting //net owners.
  static scoped_refptr<X509Certificate> CreateFromPickleUnsafeOptions(
      base::PickleIterator* pickle_iter,
      UnsafeCreateOptions options);

  // Parses all of the certificates possible from |data|. |format| is a
  // bit-wise OR of Format, indicating the possible formats the
  // certificates may have been serialized as. If an error occurs, an empty
  // collection will be returned.
  static CertificateList CreateCertificateListFromBytes(const char* data,
                                                        size_t length,
                                                        int format);

  // Appends a representation of this object to the given pickle.
  // The Pickle contains the certificate and any certificates that were
  // stored in |intermediate_ca_certs_| at the time it was serialized.
  // The format is [int count], [data - this certificate],
  // [data - intermediate1], ... [data - intermediateN].
  // All certificates are stored in DER form.
  void Persist(base::Pickle* pickle) const;

  // The serial number, DER encoded, possibly including a leading 00 byte.
  const std::string& serial_number() const { return serial_number_; }

  // The subject of the certificate.  For HTTPS server certificates, this
  // represents the web server.  The common name of the subject should match
  // the host name of the web server.
  const CertPrincipal& subject() const { return subject_; }

  // The issuer of the certificate.
  const CertPrincipal& issuer() const { return issuer_; }

  // Time period during which the certificate is valid.  More precisely, this
  // certificate is invalid before the |valid_start| date and invalid after
  // the |valid_expiry| date.
  // If we were unable to parse either date from the certificate (or if the cert
  // lacks either date), the date will be null (i.e., is_null() will be true).
  const base::Time& valid_start() const { return valid_start_; }
  const base::Time& valid_expiry() const { return valid_expiry_; }

  // Gets the subjectAltName extension field from the certificate, if any.
  // For future extension; currently this only returns those name types that
  // are required for HTTP certificate name verification - see VerifyHostname.
  // Returns true if any dNSName or iPAddress SAN was present. If |dns_names|
  // is non-null, it will be set to all dNSNames present. If |ip_addrs| is
  // non-null, it will be set to all iPAddresses present.
  bool GetSubjectAltName(std::vector<std::string>* dns_names,
                         std::vector<std::string>* ip_addrs) const;

  // Convenience method that returns whether this certificate has expired as of
  // now.
  bool HasExpired() const;

  // Returns true if this object and |other| represent the same certificate.
  // Does not consider any associated intermediates.
  bool EqualsExcludingChain(const X509Certificate* other) const;

  // Returns true if this object and |other| represent the same certificate
  // and intermediates.
  bool EqualsIncludingChain(const X509Certificate* other) const;

  // Do any of the given issuer names appear in this cert's chain of trust?
  // |valid_issuers| is a list of DER-encoded X.509 DistinguishedNames.
  bool IsIssuedByEncoded(const std::vector<std::string>& valid_issuers) const;

  // Verifies that |hostname| matches this certificate.
  // Does not verify that the certificate is valid, only that the certificate
  // matches this host.
  bool VerifyNameMatch(const std::string& hostname) const;

  // Returns the PEM encoded data from a DER encoded certificate. If the
  // return value is true, then the PEM encoded certificate is written to
  // |pem_encoded|.
  static bool GetPEMEncodedFromDER(base::StringPiece der_encoded,
                                   std::string* pem_encoded);

  // Returns the PEM encoded data from a CRYPTO_BUFFER. If the return value is
  // true, then the PEM encoded certificate is written to |pem_encoded|.
  static bool GetPEMEncoded(const CRYPTO_BUFFER* cert_buffer,
                            std::string* pem_encoded);

  // Encodes the entire certificate chain (this certificate and any
  // intermediate certificates stored in |intermediate_ca_certs_|) as a series
  // of PEM encoded strings. Returns true if all certificates were encoded,
  // storing the result in |*pem_encoded|, with this certificate stored as
  // the first element.
  bool GetPEMEncodedChain(std::vector<std::string>* pem_encoded) const;

  // Sets |*size_bits| to be the length of the public key in bits, and sets
  // |*type| to one of the |PublicKeyType| values. In case of
  // |kPublicKeyTypeUnknown|, |*size_bits| will be set to 0.
  static void GetPublicKeyInfo(const CRYPTO_BUFFER* cert_buffer,
                               size_t* size_bits,
                               PublicKeyType* type);

  // Returns the CRYPTO_BUFFER holding this certificate's DER encoded data. The
  // data is not guaranteed to be valid DER or to encode a valid Certificate
  // object.
  CRYPTO_BUFFER* cert_buffer() const { return cert_buffer_.get(); }

  // Returns the associated intermediate certificates that were specified
  // during creation of this object, if any. The intermediates are not
  // guaranteed to be valid DER or to encode valid Certificate objects.
  // Ownership follows the "get" rule: it is the caller's responsibility to
  // retain the elements of the result.
  const std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>& intermediate_buffers()
      const {
    return intermediate_ca_certs_;
  }

  // Creates a CRYPTO_BUFFER from the DER-encoded representation. Unlike
  // creating a CRYPTO_BUFFER directly, this function does some minimal
  // checking to reject obviously invalid inputs.
  // Returns NULL on failure.
  static bssl::UniquePtr<CRYPTO_BUFFER> CreateCertBufferFromBytes(
      const char* data,
      size_t length);

  // Creates all possible CRYPTO_BUFFERs from |data| encoded in a specific
  // |format|. Returns an empty collection on failure.
  static std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>
  CreateCertBuffersFromBytes(const char* data, size_t length, Format format);

  // Calculates the SHA-256 fingerprint of the certificate.  Returns an empty
  // (all zero) fingerprint on failure.
  static SHA256HashValue CalculateFingerprint256(
      const CRYPTO_BUFFER* cert_buffer);

  // Calculates the SHA-256 fingerprint for the complete chain, including the
  // leaf certificate and all intermediate CA certificates. Returns an empty
  // (all zero) fingerprint on failure.
  SHA256HashValue CalculateChainFingerprint256() const;

  // Returns true if the certificate is self-signed.
  static bool IsSelfSigned(const CRYPTO_BUFFER* cert_buffer);

 private:
  friend class base::RefCountedThreadSafe<X509Certificate>;
  friend class TestRootCerts;  // For unit tests

  FRIEND_TEST_ALL_PREFIXES(X509CertificateNameVerifyTest, VerifyHostname);
  FRIEND_TEST_ALL_PREFIXES(X509CertificateTest, SerialNumbers);

  // Construct an X509Certificate from a CRYPTO_BUFFER containing the
  // DER-encoded representation.
  X509Certificate(bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer,
                  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates);
  X509Certificate(bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer,
                  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates,
                  UnsafeCreateOptions options);

  ~X509Certificate();

  // Common object initialization code.  Called by the constructors only.
  bool Initialize(UnsafeCreateOptions options);

  // Verifies that |hostname| matches one of the certificate names or IP
  // addresses supplied, based on TLS name matching rules - specifically,
  // following http://tools.ietf.org/html/rfc6125.
  // The members of |cert_san_dns_names| and |cert_san_ipaddrs| must be filled
  // from the dNSName and iPAddress components of the subject alternative name
  // extension, if present. Note these IP addresses are NOT ascii-encoded:
  // they must be 4 or 16 bytes of network-ordered data, for IPv4 and IPv6
  // addresses, respectively.
  static bool VerifyHostname(const std::string& hostname,
                             const std::vector<std::string>& cert_san_dns_names,
                             const std::vector<std::string>& cert_san_ip_addrs);

  // The subject of the certificate.
  CertPrincipal subject_;

  // The issuer of the certificate.
  CertPrincipal issuer_;

  // This certificate is not valid before |valid_start_|
  base::Time valid_start_;

  // This certificate is not valid after |valid_expiry_|
  base::Time valid_expiry_;

  // The serial number of this certificate, DER encoded.
  std::string serial_number_;

  // A handle to the DER encoded certificate data.
  bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer_;

  // Untrusted intermediate certificates associated with this certificate
  // that may be needed for chain building.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediate_ca_certs_;

  DISALLOW_COPY_AND_ASSIGN(X509Certificate);
};

}  // namespace net

#endif  // NET_CERT_X509_CERTIFICATE_H_
