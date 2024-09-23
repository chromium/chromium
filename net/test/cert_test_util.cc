// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_test_util.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace net {

CertificateList CreateCertificateListFromFile(const base::FilePath& certs_dir,
                                              std::string_view cert_file,
                                              int format) {
  base::FilePath cert_path = certs_dir.AppendASCII(cert_file);
  std::string cert_data;
  if (!base::ReadFileToString(cert_path, &cert_data))
    return CertificateList();
  return X509Certificate::CreateCertificateListFromBytes(
      base::as_byte_span(cert_data), format);
}

::testing::AssertionResult LoadCertificateFiles(
    const std::vector<std::string>& cert_filenames,
    CertificateList* certs) {
  certs->clear();
  for (const std::string& filename : cert_filenames) {
    scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), filename, X509Certificate::FORMAT_AUTO);
    if (!cert)
      return ::testing::AssertionFailure()
             << "Failed loading certificate from file: " << filename
             << " (in directory: " << GetTestCertsDirectory().value() << ")";
    certs->push_back(cert);
  }

  return ::testing::AssertionSuccess();
}

scoped_refptr<X509Certificate> CreateCertificateChainFromFile(
    const base::FilePath& certs_dir,
    std::string_view cert_file,
    int format) {
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, cert_file, format);
  if (certs.empty())
    return nullptr;

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (size_t i = 1; i < certs.size(); ++i)
    intermediates.push_back(bssl::UpRef(certs[i]->cert_buffer()));

  scoped_refptr<X509Certificate> result(X509Certificate::CreateFromBuffer(
      bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates)));
  return result;
}

scoped_refptr<X509Certificate> ImportCertFromFile(
    const base::FilePath& cert_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string cert_data;
  if (!base::ReadFileToString(cert_path, &cert_data))
    return nullptr;

  CertificateList certs_in_file =
      X509Certificate::CreateCertificateListFromBytes(
          base::as_byte_span(cert_data), X509Certificate::FORMAT_AUTO);
  if (certs_in_file.empty())
    return nullptr;
  return certs_in_file[0];
}

scoped_refptr<X509Certificate> ImportCertFromFile(
    const base::FilePath& certs_dir,
    std::string_view cert_file) {
  return ImportCertFromFile(certs_dir.AppendASCII(cert_file));
}

ScopedTestEVPolicy::ScopedTestEVPolicy(EVRootCAMetadata* ev_root_ca_metadata,
                                       const SHA256HashValue& fingerprint,
                                       const char* policy)
    : fingerprint_(fingerprint), ev_root_ca_metadata_(ev_root_ca_metadata) {
  EXPECT_TRUE(ev_root_ca_metadata->AddEVCA(fingerprint, policy));
}

ScopedTestEVPolicy::~ScopedTestEVPolicy() {
  EXPECT_TRUE(ev_root_ca_metadata_->RemoveEVCA(fingerprint_));
}

}  // namespace net
