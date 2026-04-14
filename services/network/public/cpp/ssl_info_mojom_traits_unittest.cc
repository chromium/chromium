// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ssl_info_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/hash_value.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/ssl_info.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::Matcher;
using ::testing::Pointee;

MATCHER_P(CertEquals, expected, "") {
  if (!arg && !expected) {
    return true;
  }
  if (!arg || !expected) {
    *result_listener << (arg ? "is not null" : "is null");
    return false;
  }
  if (!arg->EqualsIncludingChain(expected.get())) {
    *result_listener << "certificates do not match";
    return false;
  }
  return true;
}

Matcher<const net::ct::DigitallySigned&> DigitallySignedEquals(
    const net::ct::DigitallySigned& expected) {
  return AllOf(
      Field("hash_algorithm", &net::ct::DigitallySigned::hash_algorithm,
            expected.hash_algorithm),
      Field("signature_algorithm",
            &net::ct::DigitallySigned::signature_algorithm,
            expected.signature_algorithm),
      Field("signature_data", &net::ct::DigitallySigned::signature_data,
            expected.signature_data));
}

Matcher<scoped_refptr<net::ct::SignedCertificateTimestamp>> SCTEquals(
    scoped_refptr<net::ct::SignedCertificateTimestamp> expected) {
  if (!expected) {
    return IsNull();
  }
  return Pointee(AllOf(
      Field("version", &net::ct::SignedCertificateTimestamp::version,
            expected->version),
      Field("log_id", &net::ct::SignedCertificateTimestamp::log_id,
            expected->log_id),
      Field("timestamp", &net::ct::SignedCertificateTimestamp::timestamp,
            expected->timestamp),
      Field("extensions", &net::ct::SignedCertificateTimestamp::extensions,
            expected->extensions),
      Field("signature", &net::ct::SignedCertificateTimestamp::signature,
            DigitallySignedEquals(expected->signature)),
      Field("origin", &net::ct::SignedCertificateTimestamp::origin,
            expected->origin),
      Field("log_description",
            &net::ct::SignedCertificateTimestamp::log_description,
            expected->log_description)));
}

Matcher<const net::SignedCertificateTimestampAndStatus&> SCTAndStatusEquals(
    const net::SignedCertificateTimestampAndStatus& expected) {
  return AllOf(
      Field("status", &net::SignedCertificateTimestampAndStatus::status,
            expected.status),
      Field("sct", &net::SignedCertificateTimestampAndStatus::sct,
            SCTEquals(expected.sct)));
}

Matcher<const net::SSLInfo&> SSLInfoEquals(const net::SSLInfo& expected) {
  std::vector<Matcher<const net::SignedCertificateTimestampAndStatus&>>
      sct_matchers;
  for (const auto& sct : expected.signed_certificate_timestamps) {
    sct_matchers.push_back(SCTAndStatusEquals(sct));
  }

  return AllOf(
      Field(&net::SSLInfo::cert, CertEquals(expected.cert)),
      Field(&net::SSLInfo::unverified_cert,
            CertEquals(expected.unverified_cert)),
      Field(&net::SSLInfo::cert_status, expected.cert_status),
      Field(&net::SSLInfo::key_exchange_group, expected.key_exchange_group),
      Field(&net::SSLInfo::peer_signature_algorithm,
            expected.peer_signature_algorithm),
      Field(&net::SSLInfo::connection_status, expected.connection_status),
      Field(&net::SSLInfo::is_issued_by_known_root,
            expected.is_issued_by_known_root),
      Field(&net::SSLInfo::pkp_bypassed, expected.pkp_bypassed),
      Field(&net::SSLInfo::client_cert_sent, expected.client_cert_sent),
      // TODO(crbug.com/501919014): These two fields are not sent over IPC
      // today.
      Field(&net::SSLInfo::early_data_received, false),
      Field(&net::SSLInfo::early_data_accepted, false),
      Field(&net::SSLInfo::encrypted_client_hello,
            expected.encrypted_client_hello),
      Field(&net::SSLInfo::handshake_type, expected.handshake_type),
      Field(&net::SSLInfo::public_key_hashes, expected.public_key_hashes),
      Field(&net::SSLInfo::signed_certificate_timestamps,
            ElementsAreArray(sct_matchers)),
      Field(&net::SSLInfo::ct_policy_compliance, expected.ct_policy_compliance),
      Field(&net::SSLInfo::is_fatal_cert_error, expected.is_fatal_cert_error));
}

}  // namespace

TEST(SSLInfoMojoTraitsTest, Default) {
  net::SSLInfo in;
  net::SSLInfo out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::SSLInfo>(in, out));
  EXPECT_THAT(out, SSLInfoEquals(in));
}

// Test serializing a SSLInfo with non-default values to validate serialization
// is actually doing something. Avoid false for booleans as that's the default
// value.
TEST(SSLInfoMojoTraitsTest, NonDefault) {
  net::SSLInfo in;
  in.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  in.unverified_cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                               "ok_cert_by_intermediate.pem");
  in.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
  in.key_exchange_group = 1024;
  in.peer_signature_algorithm = 0x0804;
  in.connection_status = 0x300039;  // TLS_DHE_RSA_WITH_AES_256_CBC_SHA
  in.is_issued_by_known_root = true;
  in.pkp_bypassed = true;
  in.client_cert_sent = true;
  in.handshake_type = net::SSLInfo::HANDSHAKE_FULL;
  const net::SHA256HashValue kCertPublicKeyHashValue = {{0x01, 0x02}};
  in.public_key_hashes.push_back(kCertPublicKeyHashValue);
  in.encrypted_client_hello = true;

  scoped_refptr<net::ct::SignedCertificateTimestamp> sct(
      new net::ct::SignedCertificateTimestamp());
  sct->version = net::ct::SignedCertificateTimestamp::V1;
  sct->log_id = "unknown_log_id";
  sct->extensions = "extensions";
  sct->timestamp = base::Time::Now();
  sct->signature.hash_algorithm = net::ct::DigitallySigned::HASH_ALGO_MD5;
  sct->signature.signature_algorithm = net::ct::DigitallySigned::SIG_ALGO_RSA;
  sct->signature.signature_data = "signature";
  sct->origin = net::ct::SignedCertificateTimestamp::SCT_EMBEDDED;
  in.signed_certificate_timestamps.push_back(
      net::SignedCertificateTimestampAndStatus(
          sct, net::ct::SCT_STATUS_LOG_UNKNOWN));

  in.ct_policy_compliance =
      net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;

  net::SSLInfo out;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::SSLInfo>(in, out));

  EXPECT_THAT(out, SSLInfoEquals(in));
}
