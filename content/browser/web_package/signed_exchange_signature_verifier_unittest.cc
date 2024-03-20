// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_verifier.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/web_package/signed_exchange_certificate_chain.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_signature_header_field.h"
#include "content/public/common/content_paths.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const uint64_t kSignatureHeaderDate = 1517892341;
const uint64_t kSignatureHeaderExpires = 1517895941;

// See content/test/data/sxg/README on how to generate these data.
// clang-format off
constexpr char kSignatureHeaderECDSAP256[] = R"(label;cert-sha256=*Pk1v56luvimpV9pI8xUFDtIh5jJb5T7LEmCnwvC5p2U=*;cert-url="https://example.com/cert.msg";date=1517892341;expires=1517895941;integrity="digest/mi-sha256-03";sig=*MEQCID7HTBrxbUl1n0dVg0S7DtF2DatBiYBzKQCAjHgrmL2YAiBjTLJbkQ0HQBexcpkDxhhCJZN8qgZUS2CDcHO7r48DMQ==*;validity-url="https://test.example.org/resource.validity.msg")";
constexpr uint8_t kCborHeadersECDSAP256[] = {
  0xa4, 0x46, 0x64, 0x69, 0x67, 0x65, 0x73, 0x74, 0x58, 0x39, 0x6d, 0x69,
  0x2d, 0x73, 0x68, 0x61, 0x32, 0x35, 0x36, 0x2d, 0x30, 0x33, 0x3d, 0x77,
  0x6d, 0x70, 0x34, 0x64, 0x52, 0x4d, 0x59, 0x67, 0x78, 0x50, 0x33, 0x74,
  0x53, 0x4d, 0x43, 0x77, 0x56, 0x2f, 0x49, 0x30, 0x43, 0x57, 0x4f, 0x43,
  0x69, 0x48, 0x5a, 0x70, 0x41, 0x69, 0x68, 0x4b, 0x5a, 0x6b, 0x31, 0x39,
  0x62, 0x73, 0x4e, 0x39, 0x52, 0x49, 0x3d, 0x47, 0x3a, 0x73, 0x74, 0x61,
  0x74, 0x75, 0x73, 0x43, 0x32, 0x30, 0x30, 0x4c, 0x63, 0x6f, 0x6e, 0x74,
  0x65, 0x6e, 0x74, 0x2d, 0x74, 0x79, 0x70, 0x65, 0x58, 0x18, 0x74, 0x65,
  0x78, 0x74, 0x2f, 0x68, 0x74, 0x6d, 0x6c, 0x3b, 0x20, 0x63, 0x68, 0x61,
  0x72, 0x73, 0x65, 0x74, 0x3d, 0x75, 0x74, 0x66, 0x2d, 0x38, 0x50, 0x63,
  0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e, 0x63, 0x6f, 0x64,
  0x69, 0x6e, 0x67, 0x4c, 0x6d, 0x69, 0x2d, 0x73, 0x68, 0x61, 0x32, 0x35,
  0x36, 0x2d, 0x30, 0x33
};
constexpr char kSignatureHeaderECDSAP384[] = R"(label;cert-sha256=*zXJfOCr77C3XNWxrPrhWNh8nsLK4jhW5neDBRIzario=*;cert-url="https://example.com/cert.msg";date=1517892341;expires=1517895941;integrity="digest/mi-sha256-03";sig=*MGQCMDLDn/k5ToXnmxOOcL80NAU6JrLUNfXvE05BdTN0N67z9fFeoZiCID+5x9oapey7SgIwOYhaIX2Lbm0i0wCY6+WSbGgsgp9HWu+utJhXJYLR4cFkCxLEpMCmARyeDaGQvWzU*;validity-url="https://test.example.org/resource.validity.msg")";
// clang-format on

// |expires| (1518497142) is more than 7 days (604800 seconds) after |date|
// (1517892341).
// clang-format off
constexpr char kSignatureHeaderInvalidExpires[] =
    "sig; "
    "sig=*RhjjWuXi87riQUu90taBHFJgTo8XBhiCe9qTJMP7/XVPu2diRGipo06HoGsyXkidHiiW"
    "743JgoNmO7CjfeVXLXQgKDxtGidATtPsVadAT4JpBDZJWSUg5qAbWcASXjyO38Uhq9gJkeu4w"
    "1MRMGkvpgVXNjYhi5/9NUer1xEUuJh5UbIDhGrfMihwj+c30nW+qz0n5lCrYonk+Sc0jGcLgc"
    "aDLptqRhOG5S+avwKmbQoqtD0JSc/53L5xXjppyvSA2fRmoDlqVQpX4uzRKq9cny7fZ3qgpZ/"
    "YOCuT7wMj7oVEur175QLe2F8ktKH9arSEiquhFJxBIIIXza8PJnmL5w==*;"
    "validity-url=\"https://example.com/resource.validity.msg\"; "
    "integrity=\"mi-draft2\"; "
    "cert-url=\"https://example.com/cert.msg\"; "
    "cert-sha256=*3wfzkF4oKGUwoQ0rE7U11FIdcA/8biGzlaACeRQQH6k=*; "
    "date=1517892341; expires=1518497142";
// clang-format on

scoped_refptr<net::X509Certificate> LoadCertificate(
    const std::string& cert_file) {
  base::FilePath dir_path;
  base::PathService::Get(content::DIR_TEST_DATA, &dir_path);
  dir_path = dir_path.AppendASCII("sxg");

  return net::CreateCertificateChainFromFile(
      dir_path, cert_file, net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
}

}  // namespace

class SignedExchangeSignatureVerifierTest
    : public ::testing::TestWithParam<SignedExchangeVersion> {
 protected:
  SignedExchangeSignatureVerifierTest() {}

  const base::Time VerificationTime() {
    return base::Time::UnixEpoch() + base::Seconds(kSignatureHeaderDate);
  }

  void TestVerifierGivenValidInput(
      const SignedExchangeEnvelope& envelope,
      scoped_refptr<net::X509Certificate> certificate) {
    SignedExchangeCertificateChain cert_chain(
        certificate, std::string() /* ocsp */, std::string() /* sct */);
    {
      base::HistogramTester histogram_tester;
      EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kSuccess,
                SignedExchangeSignatureVerifier::Verify(
                    GetParam(), envelope, &cert_chain, VerificationTime(),
                    nullptr /* devtools_proxy */));
      histogram_tester.ExpectUniqueSample(
          "SignedExchange.TimeUntilExpiration",
          kSignatureHeaderExpires - kSignatureHeaderDate, 1);
      histogram_tester.ExpectTotalCount(
          "SignedExchange.SignatureVerificationError.NotYetValid", 0);
      histogram_tester.ExpectTotalCount(
          "SignedExchange.SignatureVerificationError.Expired", 0);
    }
    {
      base::HistogramTester histogram_tester;
      EXPECT_EQ(
          SignedExchangeSignatureVerifier::Result::kErrFutureDate,
          SignedExchangeSignatureVerifier::Verify(
              GetParam(), envelope, &cert_chain,
              base::Time::UnixEpoch() + base::Seconds(kSignatureHeaderDate - 1),
              nullptr /* devtools_proxy */
              ));
      histogram_tester.ExpectTotalCount("SignedExchange.TimeUntilExpiration",
                                        0);
      histogram_tester.ExpectUniqueSample(
          "SignedExchange.SignatureVerificationError.NotYetValid", 1, 1);
      histogram_tester.ExpectTotalCount(
          "SignedExchange.SignatureVerificationError.Expired", 0);
    }

    {
      base::HistogramTester histogram_tester;
      EXPECT_EQ(
          SignedExchangeSignatureVerifier::Result::kSuccess,
          SignedExchangeSignatureVerifier::Verify(
              GetParam(), envelope, &cert_chain,
              base::Time::UnixEpoch() + base::Seconds(kSignatureHeaderExpires),
              nullptr /* devtools_proxy */
              ));
      histogram_tester.ExpectUniqueSample("SignedExchange.TimeUntilExpiration",
                                          0, 1);
      histogram_tester.ExpectTotalCount(
          "SignedExchange.SignatureVerificationError.NotYetValid", 0);
      histogram_tester.ExpectTotalCount(
          "SignedExchange.SignatureVerificationError.Expired", 0);
    }
    {
      base::HistogramTester histogram_tester;
      EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrExpired,
                SignedExchangeSignatureVerifier::Verify(
                    GetParam(), envelope, &cert_chain,
                    base::Time::UnixEpoch() +
                        base::Seconds(kSignatureHeaderExpires + 1),
                    nullptr /* devtools_proxy */
                    ));
      histogram_tester.ExpectTotalCount("SignedExchange.TimeUntilExpiration",
                                        0);
      histogram_tester.ExpectTotalCount(
          "SignedExchange.SignatureVerificationError.NotYetValid", 0);
      histogram_tester.ExpectUniqueSample(
          "SignedExchange.SignatureVerificationError.Expired", 1, 1);
    }

    SignedExchangeEnvelope invalid_expires_envelope(envelope);
    auto invalid_expires_signature =
        SignedExchangeSignatureHeaderField::ParseSignature(
            kSignatureHeaderInvalidExpires, nullptr /* devtools_proxy */);
    ASSERT_TRUE(invalid_expires_signature.has_value());
    ASSERT_EQ(1u, invalid_expires_signature->size());
    invalid_expires_envelope.SetSignatureForTesting(
        (*invalid_expires_signature)[0]);
    EXPECT_EQ(
        SignedExchangeSignatureVerifier::Result::kErrValidityPeriodTooLong,
        SignedExchangeSignatureVerifier::Verify(
            GetParam(), invalid_expires_envelope, &cert_chain,
            VerificationTime(), nullptr /* devtools_proxy */
            ));

    SignedExchangeEnvelope corrupted_envelope(envelope);
    corrupted_envelope.set_request_url(signed_exchange_utils::URLWithRawString(
        "https://example.com/bad.html"));
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::
                  kErrSignatureVerificationFailed,
              SignedExchangeSignatureVerifier::Verify(
                  GetParam(), corrupted_envelope, &cert_chain,
                  VerificationTime(), nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope badsig_envelope(envelope);
    SignedExchangeSignatureHeaderField::Signature badsig = envelope.signature();
    badsig.sig[0]++;
    badsig_envelope.SetSignatureForTesting(badsig);
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::
                  kErrSignatureVerificationFailed,
              SignedExchangeSignatureVerifier::Verify(
                  GetParam(), badsig_envelope, &cert_chain, VerificationTime(),
                  nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope badsigsha256_envelope(envelope);
    SignedExchangeSignatureHeaderField::Signature badsigsha256 =
        envelope.signature();
    badsigsha256.cert_sha256->data[0]++;
    badsigsha256_envelope.SetSignatureForTesting(badsigsha256);
    EXPECT_EQ(
        SignedExchangeSignatureVerifier::Result::kErrCertificateSHA256Mismatch,
        SignedExchangeSignatureVerifier::Verify(
            GetParam(), badsigsha256_envelope, &cert_chain, VerificationTime(),
            nullptr /* devtools_proxy */
            ));
  }
};

TEST_P(SignedExchangeSignatureVerifierTest, VerifyECDSAP256) {
  auto signature = SignedExchangeSignatureHeaderField::ParseSignature(
      kSignatureHeaderECDSAP256, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  scoped_refptr<net::X509Certificate> cert =
      LoadCertificate("prime256v1-sha256.public.pem");

  SignedExchangeEnvelope envelope;
  envelope.set_request_url(signed_exchange_utils::URLWithRawString(
      "https://test.example.org/test/"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256-03");
  envelope.AddResponseHeader(
      "digest", "mi-sha256-03=wmp4dRMYgxP3tSMCwV/I0CWOCiHZpAihKZk19bsN9RI=");
  envelope.set_cbor_header(base::make_span(kCborHeadersECDSAP256));

  envelope.SetSignatureForTesting((*signature)[0]);

  TestVerifierGivenValidInput(envelope, cert);
}

TEST_P(SignedExchangeSignatureVerifierTest, VerifyECDSAP384) {
  auto signature = SignedExchangeSignatureHeaderField::ParseSignature(
      kSignatureHeaderECDSAP384, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  scoped_refptr<net::X509Certificate> cert =
      LoadCertificate("secp384r1-sha256.public.pem");
  SignedExchangeCertificateChain cert_chain(cert, std::string() /* ocsp */,
                                            std::string() /* sct */);

  SignedExchangeEnvelope envelope;
  envelope.set_request_url(signed_exchange_utils::URLWithRawString(
      "https://test.example.org/test/"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256-03");
  envelope.AddResponseHeader(
      "digest", "mi-sha256-03=wmp4dRMYgxP3tSMCwV/I0CWOCiHZpAihKZk19bsN9RIG=");

  envelope.SetSignatureForTesting((*signature)[0]);

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrUnsupportedCertType,
            SignedExchangeSignatureVerifier::Verify(
                GetParam(), envelope, &cert_chain, VerificationTime(),
                nullptr /* devtools_proxy */));
}

INSTANTIATE_TEST_SUITE_P(SignedExchangeSignatureVerifierTests,
                         SignedExchangeSignatureVerifierTest,
                         ::testing::Values(SignedExchangeVersion::kB3));

}  // namespace content
