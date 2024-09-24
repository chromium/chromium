// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_client.h"

#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

TEST(QuicSimpleClientTest, Initialize) {
  base::test::TaskEnvironment task_environment;
  quic::QuicSocketAddress server_address(quic::QuicIpAddress::Loopback4(), 80);
  quic::QuicServerId server_id("hostname", server_address.port());
  quic::ParsedQuicVersionVector versions = quic::AllSupportedVersions();
  QuicSimpleClient client(
      server_address, server_id, versions, quic::QuicConfig(),
      quic::test::crypto_test_utils::ProofVerifierForTesting());
  EXPECT_TRUE(client.Initialize());
}

}  // namespace net::test
