// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_CERT_TEST_UTIL_H_
#define NET_TEST_CERT_TEST_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "crypto/crypto_buildflags.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/cert/scoped_nss_types.h"

// From <pk11pub.h>
typedef struct PK11SlotInfoStr PK11SlotInfo;

#include "net/cert/scoped_nss_types.h"
#endif

namespace base {
class FilePath;
}

namespace net {

class EVRootCAMetadata;

#if BUILDFLAG(USE_NSS_CERTS)
// Imports a private key from file |key_filename| in |dir| into |slot|. The file
// must contain a PKCS#8 PrivateKeyInfo in DER encoding. Returns true on success
// and false on failure.
bool ImportSensitiveKeyFromFile(const base::FilePath& dir,
                                std::string_view key_filename,
                                PK11SlotInfo* slot);

bool ImportClientCertToSlot(CERTCertificate* cert, PK11SlotInfo* slot);

ScopedCERTCertificate ImportClientCertToSlot(
    const scoped_refptr<X509Certificate>& cert,
    PK11SlotInfo* slot);

scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    std::string_view cert_filename,
    std::string_view key_filename,
    PK11SlotInfo* slot,
    ScopedCERTCertificate* nss_cert);
scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    std::string_view cert_filename,
    std::string_view key_filename,
    PK11SlotInfo* slot);

ScopedCERTCertificate ImportCERTCertificateFromFile(
    const base::FilePath& certs_dir,
    std::string_view cert_file);

ScopedCERTCertificateList CreateCERTCertificateListFromFile(
    const base::FilePath& certs_dir,
    std::string_view cert_file,
    int format);

// Returns an NSS built-in root certificate which is trusted for issuing TLS
// server certificates. If multiple ones are available, it is not specified
// which one is returned. If none are available, returns nullptr.
ScopedCERTCertificate GetAnNssBuiltinSslTrustedRoot();
#endif

// Imports all of the certificates in |cert_file|, a file in |certs_dir|, into a
// CertificateList.
CertificateList CreateCertificateListFromFile(const base::FilePath& certs_dir,
                                              std::string_view cert_file,
                                              int format);

// Imports all the certificates given a list of filenames, and assigns the
// result to |*certs|. The filenames are relative to the test certificates
// directory.
::testing::AssertionResult LoadCertificateFiles(
    const std::vector<std::string>& cert_filenames,
    CertificateList* certs);

// Imports all of the certificates in |cert_file|, a file in |certs_dir|, into
// a new X509Certificate. The first certificate in the chain will be used for
// the returned cert, with any additional certificates configured as
// intermediate certificates.
scoped_refptr<X509Certificate> CreateCertificateChainFromFile(
    const base::FilePath& certs_dir,
    std::string_view cert_file,
    int format);

// Imports a single certificate from |cert_path|.
// If the file contains multiple certificates, the first certificate found
// will be returned.
scoped_refptr<X509Certificate> ImportCertFromFile(
    const base::FilePath& cert_path);

// Imports a single certificate from |cert_file|.
// |certs_dir| represents the test certificates directory. |cert_file| is the
// name of the certificate file. If cert_file contains multiple certificates,
// the first certificate found will be returned.
scoped_refptr<X509Certificate> ImportCertFromFile(
    const base::FilePath& certs_dir,
    std::string_view cert_file);

// ScopedTestEVPolicy causes certificates marked with |policy|, issued from a
// root with the given fingerprint, to be treated as EV. |policy| is expressed
// as a string of dotted numbers: i.e. "1.2.3.4".
// This should only be used in unittests as adding a CA twice causes a CHECK
// failure.
class ScopedTestEVPolicy {
 public:
  ScopedTestEVPolicy(EVRootCAMetadata* ev_root_ca_metadata,
                     const SHA256HashValue& fingerprint,
                     const char* policy);
  ~ScopedTestEVPolicy();

 private:
  SHA256HashValue fingerprint_;
  const raw_ptr<EVRootCAMetadata> ev_root_ca_metadata_;
};

}  // namespace net

#endif  // NET_TEST_CERT_TEST_UTIL_H_
