// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/common_param_traits.h"

#include <stddef.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/pickle.h"
#include "base/values.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "content/common/content_param_traits.h"
#include "content/public/common/content_constants.h"
#include "ipc/param_traits_utils.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_policy_status.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "printing/backend/print_backend.h"
#include "printing/page_range.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

// Tests std::pair serialization
TEST(IPCMessageTest, Pair) {
  typedef std::pair<std::string, std::string> TestPair;

  TestPair input("foo", "bar");
  base::Pickle msg;
  IPC::ParamTraits<TestPair>::Write(&msg, input);

  TestPair output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<TestPair>::Read(&msg, &iter, &output));
  EXPECT_EQ(output.first, "foo");
  EXPECT_EQ(output.second, "bar");
}

// Tests bitmap serialization.
TEST(IPCMessageTest, ValueDict) {
  base::Value::Dict input;
  input.Set("null", base::Value());
  input.Set("bool", true);
  input.Set("int", 42);

  base::Value::Dict subdict;
  subdict.Set("str", "forty two");
  subdict.Set("bool", false);

  base::Value::List sublist;
  sublist.Append(42.42);
  sublist.Append("forty");
  sublist.Append("two");
  subdict.Set("list", std::move(sublist));

  input.Set("dict", std::move(subdict));

  base::Pickle msg;
  IPC::WriteParam(&msg, input);

  base::Value::Dict output;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_EQ(input, output);

  // Also test the corrupt case.
  base::Pickle bad_msg;
  bad_msg.WriteInt(99);
  iter = base::PickleIterator(bad_msg);
  EXPECT_FALSE(IPC::ReadParam(&bad_msg, &iter, &output));
}

// Tests net::SSLInfo serialization
TEST(IPCMessageTest, SSLInfo) {
  // Build a SSLInfo. Avoid false for booleans as that's the default value.
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
  in.ocsp_result.response_status = bssl::OCSPVerifyResult::PROVIDED;
  in.ocsp_result.revocation_status = bssl::OCSPRevocationStatus::REVOKED;

  // Now serialize and deserialize.
  base::Pickle msg;
  IPC::ParamTraits<net::SSLInfo>::Write(&msg, in);

  net::SSLInfo out;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<net::SSLInfo>::Read(&msg, &iter, &out));

  // Now verify they're equal.
  ASSERT_TRUE(in.cert->EqualsIncludingChain(out.cert.get()));
  ASSERT_TRUE(
      in.unverified_cert->EqualsIncludingChain(out.unverified_cert.get()));
  ASSERT_EQ(in.key_exchange_group, out.key_exchange_group);
  ASSERT_EQ(in.peer_signature_algorithm, out.peer_signature_algorithm);
  ASSERT_EQ(in.connection_status, out.connection_status);
  ASSERT_EQ(in.is_issued_by_known_root, out.is_issued_by_known_root);
  ASSERT_EQ(in.pkp_bypassed, out.pkp_bypassed);
  ASSERT_EQ(in.client_cert_sent, out.client_cert_sent);
  ASSERT_EQ(in.handshake_type, out.handshake_type);
  ASSERT_EQ(in.public_key_hashes, out.public_key_hashes);
  ASSERT_EQ(in.encrypted_client_hello, out.encrypted_client_hello);

  ASSERT_EQ(in.signed_certificate_timestamps.size(),
            out.signed_certificate_timestamps.size());
  ASSERT_EQ(in.signed_certificate_timestamps[0].status,
            out.signed_certificate_timestamps[0].status);
  ASSERT_EQ(in.signed_certificate_timestamps[0].sct->version,
            out.signed_certificate_timestamps[0].sct->version);
  ASSERT_EQ(in.signed_certificate_timestamps[0].sct->log_id,
            out.signed_certificate_timestamps[0].sct->log_id);
  ASSERT_EQ(in.signed_certificate_timestamps[0].sct->timestamp,
            out.signed_certificate_timestamps[0].sct->timestamp);
  ASSERT_EQ(in.signed_certificate_timestamps[0].sct->extensions,
            out.signed_certificate_timestamps[0].sct->extensions);
  ASSERT_EQ(in.signed_certificate_timestamps[0].sct->signature.hash_algorithm,
            out.signed_certificate_timestamps[0].sct->signature.hash_algorithm);
  ASSERT_EQ(
      in.signed_certificate_timestamps[0].sct->signature.signature_algorithm,
      out.signed_certificate_timestamps[0].sct->signature.signature_algorithm);
  ASSERT_EQ(in.signed_certificate_timestamps[0].sct->signature.signature_data,
            out.signed_certificate_timestamps[0].sct->signature.signature_data);
  ASSERT_EQ(in.signed_certificate_timestamps[0].sct->origin,
            out.signed_certificate_timestamps[0].sct->origin);
  ASSERT_EQ(in.signed_certificate_timestamps[0].sct->log_description,
            out.signed_certificate_timestamps[0].sct->log_description);

  ASSERT_EQ(in.ct_policy_compliance, out.ct_policy_compliance);
  ASSERT_EQ(in.ocsp_result, out.ocsp_result);
}

static constexpr viz::FrameSinkId kArbitraryFrameSinkId(1, 1);

TEST(IPCMessageTest, SurfaceInfo) {
  base::Pickle msg;
  const viz::SurfaceId kArbitrarySurfaceId(
      kArbitraryFrameSinkId,
      viz::LocalSurfaceId(3, base::UnguessableToken::Create()));
  constexpr float kArbitraryDeviceScaleFactor = 0.9f;
  const gfx::Size kArbitrarySize(65, 321);
  const viz::SurfaceInfo surface_info_in(
      kArbitrarySurfaceId, kArbitraryDeviceScaleFactor, kArbitrarySize);
  IPC::ParamTraits<viz::SurfaceInfo>::Write(&msg, surface_info_in);

  viz::SurfaceInfo surface_info_out;
  base::PickleIterator iter(msg);
  EXPECT_TRUE(
      IPC::ParamTraits<viz::SurfaceInfo>::Read(&msg, &iter, &surface_info_out));

  ASSERT_EQ(surface_info_in, surface_info_out);
}
