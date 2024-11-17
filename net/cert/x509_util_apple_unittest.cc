// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_apple.h"

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace x509_util {

namespace {

std::string BytesForSecCert(SecCertificateRef sec_cert) {
  std::string result;
  base::apple::ScopedCFTypeRef<CFDataRef> der_data(
      SecCertificateCopyData(sec_cert));
  if (!der_data) {
    ADD_FAILURE();
    return result;
  }

  return std::string(
      base::as_string_view(base::apple::CFDataToSpan(der_data.get())));
}

std::string BytesForSecCert(const void* sec_cert) {
  return BytesForSecCert(
      reinterpret_cast<SecCertificateRef>(const_cast<void*>(sec_cert)));
}

}  // namespace

TEST(X509UtilTest, CreateSecCertificateArrayForX509Certificate) {
  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "multi-root-chain1.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(cert);
  EXPECT_EQ(3U, cert->intermediate_buffers().size());

  base::apple::ScopedCFTypeRef<CFMutableArrayRef> sec_certs(
      CreateSecCertificateArrayForX509Certificate(cert.get()));
  ASSERT_TRUE(sec_certs);
  ASSERT_EQ(4, CFArrayGetCount(sec_certs.get()));
  for (int i = 0; i < 4; ++i)
    ASSERT_TRUE(CFArrayGetValueAtIndex(sec_certs.get(), i));

  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()),
            BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 0)));
  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(
                cert->intermediate_buffers()[0].get()),
            BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 1)));
  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(
                cert->intermediate_buffers()[1].get()),
            BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 2)));
  EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(
                cert->intermediate_buffers()[2].get()),
            BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 3)));
}

TEST(X509UtilTest, CreateSecCertificateArrayForX509CertificateErrors) {
  scoped_refptr<X509Certificate> ok_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  bssl::UniquePtr<CRYPTO_BUFFER> bad_cert =
      x509_util::CreateCryptoBuffer(std::string_view("invalid"));
  ASSERT_TRUE(bad_cert);

  scoped_refptr<X509Certificate> ok_cert2(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(ok_cert);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(bad_cert));
  intermediates.push_back(bssl::UpRef(ok_cert2->cert_buffer()));
  scoped_refptr<X509Certificate> cert_with_intermediates(
      X509Certificate::CreateFromBuffer(bssl::UpRef(ok_cert->cert_buffer()),
                                        std::move(intermediates)));
  ASSERT_TRUE(cert_with_intermediates);
  EXPECT_EQ(2U, cert_with_intermediates->intermediate_buffers().size());

  // With InvalidIntermediateBehavior::kIgnore, invalid intermediate certs
  // should be silently dropped.
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> sec_certs(
      CreateSecCertificateArrayForX509Certificate(
          cert_with_intermediates.get(), InvalidIntermediateBehavior::kIgnore));
  ASSERT_TRUE(sec_certs);
  for (int i = 0; i < CFArrayGetCount(sec_certs.get()); ++i)
    ASSERT_TRUE(CFArrayGetValueAtIndex(sec_certs.get(), i));

  if (CFArrayGetCount(sec_certs.get()) == 2) {
    EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(ok_cert->cert_buffer()),
              BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 0)));
    EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(ok_cert2->cert_buffer()),
              BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 1)));

    // Normal CreateSecCertificateArrayForX509Certificate should fail with
    // invalid certs in chain.
    EXPECT_FALSE(CreateSecCertificateArrayForX509Certificate(
        cert_with_intermediates.get()));
  } else if (CFArrayGetCount(sec_certs.get()) == 3) {
    // On older macOS versions that do lazy parsing of SecCertificates, the
    // invalid certificate may be accepted, which is okay. The test is just
    // verifying that *if* creating a SecCertificate from one of the
    // intermediates fails, that cert is ignored and the other certs are still
    // returned.
    EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(ok_cert->cert_buffer()),
              BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 0)));
    EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(bad_cert.get()),
              BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 1)));
    EXPECT_EQ(x509_util::CryptoBufferAsStringPiece(ok_cert2->cert_buffer()),
              BytesForSecCert(CFArrayGetValueAtIndex(sec_certs.get(), 2)));

    // Normal CreateSecCertificateArrayForX509Certificate should also
    // succeed in this case.
    EXPECT_TRUE(CreateSecCertificateArrayForX509Certificate(
        cert_with_intermediates.get()));
  } else {
    ADD_FAILURE() << "CFArrayGetCount(sec_certs.get()) = "
                  << CFArrayGetCount(sec_certs.get());
  }
}

TEST(X509UtilTest,
     CreateSecCertificateFromBytesAndCreateX509CertificateFromSecCertificate) {
  CertificateList certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "multi-root-chain1.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_EQ(4u, certs.size());

  std::string bytes_cert0(
      x509_util::CryptoBufferAsStringPiece(certs[0]->cert_buffer()));
  std::string bytes_cert1(
      x509_util::CryptoBufferAsStringPiece(certs[1]->cert_buffer()));
  std::string bytes_cert2(
      x509_util::CryptoBufferAsStringPiece(certs[2]->cert_buffer()));
  std::string bytes_cert3(
      x509_util::CryptoBufferAsStringPiece(certs[3]->cert_buffer()));

  base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert0(
      CreateSecCertificateFromBytes(base::as_byte_span(bytes_cert0)));
  ASSERT_TRUE(sec_cert0);
  EXPECT_EQ(bytes_cert0, BytesForSecCert(sec_cert0.get()));

  base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert1(
      CreateSecCertificateFromBytes(base::as_byte_span(bytes_cert1)));
  ASSERT_TRUE(sec_cert1);
  EXPECT_EQ(bytes_cert1, BytesForSecCert(sec_cert1.get()));

  base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert2(
      CreateSecCertificateFromX509Certificate(certs[2].get()));
  ASSERT_TRUE(sec_cert2);
  EXPECT_EQ(bytes_cert2, BytesForSecCert(sec_cert2.get()));

  base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert3(
      CreateSecCertificateFromX509Certificate(certs[3].get()));
  ASSERT_TRUE(sec_cert3);
  EXPECT_EQ(bytes_cert3, BytesForSecCert(sec_cert3.get()));

  scoped_refptr<X509Certificate> x509_cert_no_intermediates =
      CreateX509CertificateFromSecCertificate(sec_cert0, {});
  ASSERT_TRUE(x509_cert_no_intermediates);
  EXPECT_EQ(0U, x509_cert_no_intermediates->intermediate_buffers().size());
  EXPECT_EQ(bytes_cert0, x509_util::CryptoBufferAsStringPiece(
                             x509_cert_no_intermediates->cert_buffer()));

  scoped_refptr<X509Certificate> x509_cert_one_intermediate =
      CreateX509CertificateFromSecCertificate(sec_cert0, {sec_cert1});
  ASSERT_TRUE(x509_cert_one_intermediate);
  EXPECT_EQ(bytes_cert0, x509_util::CryptoBufferAsStringPiece(
                             x509_cert_one_intermediate->cert_buffer()));
  ASSERT_EQ(1U, x509_cert_one_intermediate->intermediate_buffers().size());
  EXPECT_EQ(bytes_cert1,
            x509_util::CryptoBufferAsStringPiece(
                x509_cert_one_intermediate->intermediate_buffers()[0].get()));

  scoped_refptr<X509Certificate> x509_cert_two_intermediates =
      CreateX509CertificateFromSecCertificate(sec_cert0,
                                              {sec_cert1, sec_cert2});
  ASSERT_TRUE(x509_cert_two_intermediates);
  EXPECT_EQ(bytes_cert0, x509_util::CryptoBufferAsStringPiece(
                             x509_cert_two_intermediates->cert_buffer()));
  ASSERT_EQ(2U, x509_cert_two_intermediates->intermediate_buffers().size());
  EXPECT_EQ(bytes_cert1,
            x509_util::CryptoBufferAsStringPiece(
                x509_cert_two_intermediates->intermediate_buffers()[0].get()));
  EXPECT_EQ(bytes_cert2,
            x509_util::CryptoBufferAsStringPiece(
                x509_cert_two_intermediates->intermediate_buffers()[1].get()));
}

}  // namespace x509_util

}  // namespace net
