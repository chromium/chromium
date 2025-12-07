// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/properties_based_quic_server_info.h"

#include <string>

#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/privacy_mode.h"
#include "net/http/http_server_properties.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

namespace {
const char kServerConfigA[] = "server_config_a";
const char kSourceAddressTokenA[] = "source_address_token_a";
const char kCertSCTA[] = "cert_sct_a";
const char kChloHashA[] = "chlo_hash_a";
const char kServerConfigSigA[] = "server_config_sig_a";
const char kCertA[] = "cert_a";
const char kCertB[] = "cert_b";
}  // namespace

class PropertiesBasedQuicServerInfoTest : public ::testing::Test {
 protected:
  PropertiesBasedQuicServerInfoTest()
      : server_id_("www.google.com", 443),
        server_info_(server_id_,
                     PRIVACY_MODE_DISABLED,
                     NetworkAnonymizationKey(),
                     &http_server_properties_) {}

  // Initialize |server_info_| object and persist it.
  void InitializeAndPersist() {
    QuicServerInfo::State* state = server_info_.mutable_state();
    EXPECT_TRUE(state->certs.empty());

    state->server_config = kServerConfigA;
    state->source_address_token = kSourceAddressTokenA;
    state->server_config_sig = kServerConfigSigA;
    state->cert_sct = kCertSCTA;
    state->chlo_hash = kChloHashA;
    state->certs.push_back(kCertA);
    server_info_.Persist();
  }

  // Verify the data that is persisted in InitializeAndPersist().
  void VerifyInitialData(const QuicServerInfo::State& state) {
    EXPECT_EQ(kServerConfigA, state.server_config);
    EXPECT_EQ(kSourceAddressTokenA, state.source_address_token);
    EXPECT_EQ(kCertSCTA, state.cert_sct);
    EXPECT_EQ(kChloHashA, state.chlo_hash);
    EXPECT_EQ(kServerConfigSigA, state.server_config_sig);
    EXPECT_EQ(kCertA, state.certs[0]);
  }

  HttpServerProperties http_server_properties_;
  quic::QuicServerId server_id_;
  PropertiesBasedQuicServerInfo server_info_;
};

// Test persisting, reading and verifying and then updating and verifing.
TEST_F(PropertiesBasedQuicServerInfoTest, Update) {
  InitializeAndPersist();

  // Read the persisted data and verify we have read the data correctly.
  PropertiesBasedQuicServerInfo server_info1(server_id_, PRIVACY_MODE_DISABLED,
                                             NetworkAnonymizationKey(),
                                             &http_server_properties_);
  EXPECT_TRUE(server_info1.Load());

  // Verify the data.
  const QuicServerInfo::State& state1 = server_info1.state();
  EXPECT_EQ(1U, state1.certs.size());
  VerifyInitialData(state1);

  // Update the data, by adding another cert.
  QuicServerInfo::State* state2 = server_info1.mutable_state();
  state2->certs.push_back(kCertB);
  server_info1.Persist();

  // Read the persisted data and verify we have read the data correctly.
  PropertiesBasedQuicServerInfo server_info2(server_id_, PRIVACY_MODE_DISABLED,
                                             NetworkAnonymizationKey(),
                                             &http_server_properties_);
  EXPECT_TRUE(server_info2.Load());

  // Verify updated data.
  const QuicServerInfo::State& state3 = server_info2.state();
  VerifyInitialData(state3);
  EXPECT_EQ(2U, state3.certs.size());
  EXPECT_EQ(kCertB, state3.certs[1]);
}

}  // namespace net::test
