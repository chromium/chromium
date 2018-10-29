// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_verifier.h"

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_signature_header_field.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const uint64_t kSignatureHeaderDate = 1517892341;
const uint64_t kSignatureHeaderExpires = 1517895941;

// See content/testdata/sxg/README on how to generate these data.
// clang-format off
constexpr char kSignatureHeaderRSA[] = R"(label; sig=*DDeXzJshGnPT+ei1rS1KmZx+QLwwLTbNKDVSmTb2HjGfgPngv+C+uMbjZiliOmGe0b514JcAlYAM57t0kZY2FPd9JdqwYPIiAWEwxByfV2iXBbsGZNWGtS/AAq1SaPwIMfrzdLXAFbKbtTRhS7B5LHCo/6hEIXu0TJJFbv5fKaLgTTLF0AK5dV0/En0uz+bnVARuBIH/ez2gPEFc6KbGnTTp8LYcCe/YjlHQy/Oac28ACBtn70rP1TerWEaYBwMMDckJ2gfsVyLqMcFtJqV0uGLT6Atb2wBSUZlZDTEZf228362r+EHLrADAuhz4bdSMKFsFgWyceOriDyHhc0PSwQ==*; validity-url="https://example.com/resource.validity.msg"; integrity="digest/mi-sha256-03"; cert-url="https://example.com/cert.msg"; cert-sha256=*tJGJP8ej7KCEW8VnVK3bKwpBza/oLrtWA75z5ZPptuc=*; date=1517892341; expires=1517895941)";
constexpr char kSignatureHeaderECDSAP256[] = R"(label; sig=*MEUCIQC7tM/B6YxVgrJmgfFawtwBKPev2vFCh7amR+JTDBMgTQIga9LkS51vteYr8NWPTCSZRy10lcLaFNN9m1G3OBS9lBs=*; validity-url="https://example.com/resource.validity.msg"; integrity="digest/mi-sha256-03"; cert-url="https://example.com/cert.msg"; cert-sha256=*KX+BYLSMgDOON8Ju65RoId39Qvajxa12HO+WnD4HpS0=*; date=1517892341; expires=1517895941)";
constexpr uint8_t kCborHeadersECDSAP256[] = {
  0x82, 0xa1, 0x47, 0x3a, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64, 0x43, 0x47,
  0x45, 0x54, 0xa4, 0x46, 0x64, 0x69, 0x67, 0x65, 0x73, 0x74, 0x58, 0x39,
  0x6d, 0x69, 0x2d, 0x73, 0x68, 0x61, 0x32, 0x35, 0x36, 0x2d, 0x30, 0x33,
  0x3d, 0x77, 0x6d, 0x70, 0x34, 0x64, 0x52, 0x4d, 0x59, 0x67, 0x78, 0x50,
  0x33, 0x74, 0x53, 0x4d, 0x43, 0x77, 0x56, 0x2f, 0x49, 0x30, 0x43, 0x57,
  0x4f, 0x43, 0x69, 0x48, 0x5a, 0x70, 0x41, 0x69, 0x68, 0x4b, 0x5a, 0x6b,
  0x31, 0x39, 0x62, 0x73, 0x4e, 0x39, 0x52, 0x49, 0x3d, 0x47, 0x3a, 0x73,
  0x74, 0x61, 0x74, 0x75, 0x73, 0x43, 0x32, 0x30, 0x30, 0x4c, 0x63, 0x6f,
  0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x74, 0x79, 0x70, 0x65, 0x58, 0x18,
  0x74, 0x65, 0x78, 0x74, 0x2f, 0x68, 0x74, 0x6d, 0x6c, 0x3b, 0x20, 0x63,
  0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x75, 0x74, 0x66, 0x2d, 0x38,
  0x50, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e, 0x63,
  0x6f, 0x64, 0x69, 0x6e, 0x67, 0x4c, 0x6d, 0x69, 0x2d, 0x73, 0x68, 0x61,
  0x32, 0x35, 0x36, 0x2d, 0x30, 0x33
};
constexpr char kSignatureHeaderECDSAP384[] = R"(label; sig=*MGUCMQDm3+Mf3ymTQOF2EUFk+NDIpOIqbFCboYsPD9YOV9rpayKTmAXzUD7Hxtp+XP/8mQECMEfTRcJmvL9QMAMKuDIzQqy/ib8MPeJHap9kQVQT1OdROaYj4EISngkJeT5om9/YlA==*; validity-url="https://example.com/resource.validity.msg"; integrity="digest/mi-sha256-03"; cert-url="https://example.com/cert.msg"; cert-sha256=*8X8y8nj8vDJHSSa0cxn+TCu+8zGpIJfbdzAnd5cW+jA=*; date=1517892341; expires=1517895941)";
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

constexpr char kCertPEMRSA[] = R"(
-----BEGIN CERTIFICATE-----
MIIDyTCCArGgAwIBAgIBBDANBgkqhkiG9w0BAQsFADBjMQswCQYDVQQGEwJVUzET
MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEQMA4G
A1UECgwHVGVzdCBDQTEVMBMGA1UEAwwMVGVzdCBSb290IENBMB4XDTE3MDYwNTE3
MTA0NloXDTI3MDYwMzE3MTA0NlowYDELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNh
bGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3Qg
Q0ExEjAQBgNVBAMMCTEyNy4wLjAuMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC
AQoCggEBANOUHzO0uxUyd3rYUArq33olXC0N1AYNM0wFTjUqUrElLiX48+5hERkG
hGwC8VG5Zr/2Jw/wtarLiDjg2OfPdwyMp3S7MBTgvXWZ989MUHpx6b0cWM298iOg
/VeinMphFLDfPDHFWZ7RXBqfk6MGLhI5GgvoooYw2jUmP+elnoizIL/OB08sIYra
AVrwasoRd+yOmyvQnzw3mZNKpWjeX7NhZCg2nG8B8u78agwAYVWupHnJS2GwhLzy
19AxU/HmaI9kyyMGmRtbRZ0roCyMDOgEEcWUSYNRP33KLi31uKYqOSblvzmC7kA7
k5yca3VXlgqg4gnjr9tbOMzMcpeqeaMCAwEAAaOBijCBhzAMBgNVHRMBAf8EAjAA
MB0GA1UdDgQWBBQYDOtRudM2qckEr/kvFPCZZtJ21DAfBgNVHSMEGDAWgBSbJguK
mKm7HbkfHOMaQDPtjheIqzAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIw
GAYDVR0RBBEwD4INKi5leGFtcGxlLm9yZzANBgkqhkiG9w0BAQsFAAOCAQEAvXK0
UF19i7JkSSdomQwB18WRFaKG8VZpSFsKbEECPRHoxktMl/Pd04wk+W0fZFq433j3
4D+cjTB6OxAVdPIPSex8U40fYMl9C13K1tejf4o/+rcLxEDdVfv7PUkogrliXzSE
MCYdcTwruV7hjC2/Ib0t/kdxblRt4dD2I1jdntsFy/VfET/m0J2qRhJWlfYEzCFe
Hn8H/PZIiIsso5pm2RodTqi9w4/+1r8Yyfmk8TF+EoWDYtbZ+ScgtCH5fldS+onI
hHgjz/tniqjbY0MRFr9ZxrohmtgOBOvROEKH06c92oOmj2ahyFpM/yU9PL/JvNmF
SaMW1eOzjHemIWKTMw==
-----END CERTIFICATE-----)";

constexpr char kCertPEMECDSAP256[] = R"(
-----BEGIN CERTIFICATE-----
MIIC1TCCAb2gAwIBAgIBATANBgkqhkiG9w0BAQsFADBjMQswCQYDVQQGEwJVUzET
MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEQMA4G
A1UECgwHVGVzdCBDQTEVMBMGA1UEAwwMVGVzdCBSb290IENBMB4XDTE4MDYyODA1
MTUzMFoXDTE5MDYyMzA1MTUzMFowNzEZMBcGA1UEAwwQdGVzdC5leGFtcGxlLm9y
ZzENMAsGA1UECgwEVGVzdDELMAkGA1UEBhMCVVMwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAAQJBifccM8+G0y/aHPKMjsTcVTz0SOfNO28t304/nkYsCxoT8UJNZvH
qso7EXs7iM/Q3c+wjOv6dPWUiLH4enG6o4GKMIGHMAkGA1UdEwQCMAAwEAYKKwYB
BAHWeQIBFgQCBQAwCwYDVR0PBAQDAgXgMB0GA1UdDgQWBBS6dTuFdAI6uylsw3cy
H3FXfh9g+jAfBgNVHSMEGDAWgBSbJguKmKm7HbkfHOMaQDPtjheIqzAbBgNVHREE
FDASghB0ZXN0LmV4YW1wbGUub3JnMA0GCSqGSIb3DQEBCwUAA4IBAQCi/l1E+JDK
/g3cLa5GD8vthZJuFwYEF6lGaAj1RtZ+UwbtRs1vnkJbEpLD1xX5rKXAdWT5QI99
yK6gXbbicaJmw0KjeE0qizTT1oEfavQu7FtJZ4gfBjIHLsk8PVqHI3t8hf/pJwOd
n+E79k3qQ2w1IeeVFZXJfnjhOsxHp2NTbeY+ZnbWsTSyUiL81n5GkuyKNDeZkoXi
x5M6kp+6ZZJHJvLQFp4CqhU+wvM2lvP5mYYDcSlRnlti+N8xwDUb/yGR0UdNx76K
7uFRoc8R1W8e4kFvU2NHkrtVbaLL6m+/vHE2LehVPh0QQT34Fv0QugYm+iYNToCT
k5bUo19UY4w3
-----END CERTIFICATE-----)";

constexpr char kCertPEMECDSAP384[] = R"(
-----BEGIN CERTIFICATE-----
MIICYDCCAUgCAQEwDQYJKoZIhvcNAQELBQAwYzELMAkGA1UEBhMCVVMxEzARBgNV
BAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoM
B1Rlc3QgQ0ExFTATBgNVBAMMDFRlc3QgUm9vdCBDQTAeFw0xODA0MDkwMTUyMzVa
Fw0xOTA0MDQwMTUyMzVaMDcxGTAXBgNVBAMMEHRlc3QuZXhhbXBsZS5vcmcxDTAL
BgNVBAoMBFRlc3QxCzAJBgNVBAYTAlVTMHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE
YK0FPc6B2UkDO3GHS95PLss9e82f8RdQDIZE9UPUSOJ1UISOT19j/SJq3gyoY+pK
J818LhVe+ywgdH+tKosO6v1l2o/EffIRDjCfN/aSUuQjkkSwgyL62/9687+486z6
MA0GCSqGSIb3DQEBCwUAA4IBAQB61Q+/68hsD5OapG+2CDsJI+oR91H+Jv+tRMby
of47O0hJGISuAB9xcFhIcMKwBReODpBmzwSO713NNU/oaG/XysHH1TNZZodTtWD9
Z1g5AJamfwvFS+ObqzOtyFUdFS4NBAE4lXi5XnHa2hU2Bkm+abVYLqyAGw1kh2ES
DGC2vA1lb2Uy9bgLCYYkZoESjb/JYRQjCmqlwYKOozU7ZbIe3zJPjRWYP1Tuany5
+rYllWk/DJlMVjs/fbf0jj32vrevCgul43iWMgprOw1ncuK8l5nND/o5aN2mwMDw
Xhe5DP7VATeQq3yGV3ps+rCTHDP6qSHDEWP7DqHQdSsxtI0E
-----END CERTIFICATE-----)";

constexpr char kPEMECDSAP256SPKIHash[] =
    "iwtEGagHhL9HbHI38aoFstFPEyB+lzZO5H2ZZAJlYOo=";
constexpr char kPEMECDSAP384SPKIHash[] =
    "aGcf7fF/2+mXuHjYen7FZ8HZPR0B6sK6zIsyrCoB6Y8=";

}  // namespace

class SignedExchangeSignatureVerifierTest : public ::testing::Test {
 protected:
  SignedExchangeSignatureVerifierTest() {}

  const base::Time VerificationTime() {
    return base::Time::UnixEpoch() +
           base::TimeDelta::FromSeconds(kSignatureHeaderDate);
  }

  void TestVerifierGivenValidInput(
      const SignedExchangeEnvelope& envelope,
      scoped_refptr<net::X509Certificate> certificate) {
    {
      base::HistogramTester histogram_tester;
      EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kSuccess,
                SignedExchangeSignatureVerifier::Verify(
                    envelope, certificate, VerificationTime(),
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
      EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
                SignedExchangeSignatureVerifier::Verify(
                    envelope, certificate,
                    base::Time::UnixEpoch() +
                        base::TimeDelta::FromSeconds(kSignatureHeaderDate - 1),
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
      EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kSuccess,
                SignedExchangeSignatureVerifier::Verify(
                    envelope, certificate,
                    base::Time::UnixEpoch() +
                        base::TimeDelta::FromSeconds(kSignatureHeaderExpires),
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
      EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
                SignedExchangeSignatureVerifier::Verify(
                    envelope, certificate,
                    base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(
                                                  kSignatureHeaderExpires + 1),
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
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp,
              SignedExchangeSignatureVerifier::Verify(
                  invalid_expires_envelope, certificate, VerificationTime(),
                  nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope corrupted_envelope(envelope);
    corrupted_envelope.set_request_url(GURL("https://example.com/bad.html"));
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::
                  kErrSignatureVerificationFailed,
              SignedExchangeSignatureVerifier::Verify(
                  corrupted_envelope, certificate, VerificationTime(),
                  nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope badsig_envelope(envelope);
    SignedExchangeSignatureHeaderField::Signature badsig = envelope.signature();
    badsig.sig[0]++;
    badsig_envelope.SetSignatureForTesting(badsig);
    EXPECT_EQ(SignedExchangeSignatureVerifier::Result::
                  kErrSignatureVerificationFailed,
              SignedExchangeSignatureVerifier::Verify(
                  badsig_envelope, certificate, VerificationTime(),
                  nullptr /* devtools_proxy */
                  ));

    SignedExchangeEnvelope badsigsha256_envelope(envelope);
    SignedExchangeSignatureHeaderField::Signature badsigsha256 =
        envelope.signature();
    badsigsha256.cert_sha256->data[0]++;
    badsigsha256_envelope.SetSignatureForTesting(badsigsha256);
    EXPECT_EQ(
        SignedExchangeSignatureVerifier::Result::kErrCertificateSHA256Mismatch,
        SignedExchangeSignatureVerifier::Verify(badsigsha256_envelope,
                                                certificate, VerificationTime(),
                                                nullptr /* devtools_proxy */
                                                ));
  }
};

TEST_F(SignedExchangeSignatureVerifierTest, VerifyRSA) {
  auto signature = SignedExchangeSignatureHeaderField::ParseSignature(
      kSignatureHeaderRSA, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  net::CertificateList certlist =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEMRSA, base::size(kCertPEMRSA),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, certlist.size());

  SignedExchangeEnvelope envelope;
  envelope.set_request_method("GET");
  envelope.set_request_url(GURL("https://test.example.org/test/"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256-03");
  envelope.AddResponseHeader(
      "digest", "mi-sha256-03=wmp4dRMYgxP3tSMCwV/I0CWOCiHZpAihKZk19bsN9RI=");
  envelope.SetSignatureForTesting((*signature)[0]);

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrUnsupportedCertType,
            SignedExchangeSignatureVerifier::Verify(
                envelope, certlist[0], VerificationTime(),
                nullptr /* devtools_proxy */));
}

TEST_F(SignedExchangeSignatureVerifierTest, VerifyECDSAP256) {
  auto signature = SignedExchangeSignatureHeaderField::ParseSignature(
      kSignatureHeaderECDSAP256, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  net::CertificateList certlist =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEMECDSAP256, base::size(kCertPEMECDSAP256),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, certlist.size());

  SignedExchangeEnvelope envelope;
  envelope.set_request_method("GET");
  envelope.set_request_url(GURL("https://test.example.org/test/"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256-03");
  envelope.AddResponseHeader(
      "digest", "mi-sha256-03=wmp4dRMYgxP3tSMCwV/I0CWOCiHZpAihKZk19bsN9RI=");
  envelope.set_cbor_header(base::make_span(kCborHeadersECDSAP256));

  envelope.SetSignatureForTesting((*signature)[0]);

  TestVerifierGivenValidInput(envelope, certlist[0]);
}

TEST_F(SignedExchangeSignatureVerifierTest, VerifyECDSAP384) {
  auto signature = SignedExchangeSignatureHeaderField::ParseSignature(
      kSignatureHeaderECDSAP384, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signature.has_value());
  ASSERT_EQ(1u, signature->size());

  net::CertificateList certlist =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEMECDSAP384, base::size(kCertPEMECDSAP384),
          net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1u, certlist.size());

  SignedExchangeEnvelope envelope;
  envelope.set_request_method("GET");
  envelope.set_request_url(GURL("https://test.example.org/test/"));
  envelope.set_response_code(net::HTTP_OK);
  envelope.AddResponseHeader("content-type", "text/html; charset=utf-8");
  envelope.AddResponseHeader("content-encoding", "mi-sha256-03");
  envelope.AddResponseHeader(
      "digest", "mi-sha256-03=wmp4dRMYgxP3tSMCwV/I0CWOCiHZpAihKZk19bsN9RIG=");

  envelope.SetSignatureForTesting((*signature)[0]);

  EXPECT_EQ(SignedExchangeSignatureVerifier::Result::kErrUnsupportedCertType,
            SignedExchangeSignatureVerifier::Verify(
                envelope, certlist[0], VerificationTime(),
                nullptr /* devtools_proxy */));
}

TEST_F(SignedExchangeSignatureVerifierTest, IgnoreErrorsSPKIList) {
  SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList ignore_nothing("");
  SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList ignore_ecdsap256(
      kPEMECDSAP256SPKIHash);
  SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList ignore_ecdsap384(
      kPEMECDSAP384SPKIHash);
  SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList ignore_both(
      std::string(kPEMECDSAP256SPKIHash) + "," + kPEMECDSAP384SPKIHash);

  scoped_refptr<net::X509Certificate> cert_ecdsap256 =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEMECDSAP256, base::size(kCertPEMECDSAP256),
          net::X509Certificate::FORMAT_AUTO)[0];
  scoped_refptr<net::X509Certificate> cert_ecdsap384 =
      net::X509Certificate::CreateCertificateListFromBytes(
          kCertPEMECDSAP384, base::size(kCertPEMECDSAP384),
          net::X509Certificate::FORMAT_AUTO)[0];

  EXPECT_FALSE(ignore_nothing.ShouldIgnoreError(cert_ecdsap256));
  EXPECT_FALSE(ignore_nothing.ShouldIgnoreError(cert_ecdsap384));
  EXPECT_TRUE(ignore_ecdsap256.ShouldIgnoreError(cert_ecdsap256));
  EXPECT_FALSE(ignore_ecdsap256.ShouldIgnoreError(cert_ecdsap384));
  EXPECT_FALSE(ignore_ecdsap384.ShouldIgnoreError(cert_ecdsap256));
  EXPECT_TRUE(ignore_ecdsap384.ShouldIgnoreError(cert_ecdsap384));
  EXPECT_TRUE(ignore_both.ShouldIgnoreError(cert_ecdsap256));
  EXPECT_TRUE(ignore_both.ShouldIgnoreError(cert_ecdsap384));
}

}  // namespace content
