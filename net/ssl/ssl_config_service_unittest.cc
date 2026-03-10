// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include <vector>

#include "base/containers/extend.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/test/cert_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

class MockSSLConfigService : public SSLConfigService {
 public:
  explicit MockSSLConfigService(const SSLContextConfig& config)
      : config_(config) {}
  ~MockSSLConfigService() override = default;

  // SSLConfigService implementation
  SSLContextConfig GetSSLContextConfig() override { return config_; }

  bool CanShareConnectionWithClientCerts(
      std::string_view hostname) const override {
    return false;
  }

  // Sets the SSLContextConfig to be returned by GetSSLContextConfig and
  // processes any updates.
  void SetSSLContextConfig(const SSLContextConfig& config) {
    SSLContextConfig old_config = config_;
    config_ = config;
    ProcessConfigUpdate(old_config, config_, /*force_notification*/ false);
  }

  using SSLConfigService::ProcessConfigUpdate;

 private:
  SSLContextConfig config_;
};

class MockSSLConfigServiceObserver : public SSLConfigService::Observer {
 public:
  MockSSLConfigServiceObserver() = default;
  ~MockSSLConfigServiceObserver() override = default;

  MOCK_METHOD0(OnSSLContextConfigChanged, void());
};

// Given a list of trust anchor ids (DER-encoded relative OIDs), returns the
// wire-encoding of the TrustAnchorIDList, as used in the trust_anchors
// extension.
std::vector<uint8_t> EncodeTrustAnchorIDs(
    const std::vector<std::vector<uint8_t>>& ids) {
  std::vector<uint8_t> ret;
  for (const auto& id : ids) {
    ret.push_back(id.size());
    base::Extend(ret, id);
  }
  return ret;
}

scoped_refptr<X509Certificate> GetTestSignaturelessMTC() {
  // The log id here doesn't matter for the purposes of these tests, the code
  // being tested only checks whether the cert is an MTC or not.
  static constexpr uint8_t kMtcLogId[] = {0x09, 0x08, 0x07};
  net::MtcLogBuilder mtc_log(kMtcLogId);
  std::unique_ptr<net::CertBuilder> mtc_leaf =
      std::move(net::CertBuilder::CreateSimpleChain(1u)[0]);
  uint64_t mtc_log_index = mtc_log.AddEntry(*mtc_leaf);
  mtc_log.AdvanceLandmark();
  auto mtc_cert_buffer =
      mtc_log.CreateSignaturelessCertificateBuffer(mtc_log_index);
  if (!mtc_cert_buffer) {
    ADD_FAILURE();
    return nullptr;
  }
  auto mtc_cert =
      X509Certificate::CreateFromBuffer(std::move(mtc_cert_buffer), {});
  return mtc_cert;
}

}  // namespace

TEST(SSLConfigServiceTest, NoChangesWontNotifyObservers) {
  SSLContextConfig initial_config;
  initial_config.version_min = SSL_PROTOCOL_VERSION_TLS1_2;
  initial_config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;

  MockSSLConfigService mock_service(initial_config);
  MockSSLConfigServiceObserver observer;
  mock_service.AddObserver(&observer);

  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(0);
  mock_service.SetSSLContextConfig(initial_config);

  mock_service.RemoveObserver(&observer);
}

TEST(SSLConfigServiceTest, ForceNotificationNotifiesObservers) {
  SSLContextConfig initial_config;
  initial_config.version_min = SSL_PROTOCOL_VERSION_TLS1_2;
  initial_config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;

  MockSSLConfigService mock_service(initial_config);
  MockSSLConfigServiceObserver observer;
  mock_service.AddObserver(&observer);

  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.ProcessConfigUpdate(initial_config, initial_config, true);

  mock_service.RemoveObserver(&observer);
}

TEST(SSLConfigServiceTest, ConfigUpdatesNotifyObservers) {
  SSLContextConfig initial_config;
  initial_config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;

  MockSSLConfigService mock_service(initial_config);
  MockSSLConfigServiceObserver observer;
  mock_service.AddObserver(&observer);

  // Test that changing the SSL version range triggers updates.
  initial_config.version_min = SSL_PROTOCOL_VERSION_TLS1_3;
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  initial_config.version_min = SSL_PROTOCOL_VERSION_TLS1_2;
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  initial_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  // Test that disabling certain cipher suites triggers an update.
  std::vector<uint16_t> disabled_ciphers;
  disabled_ciphers.push_back(0x0004u);
  disabled_ciphers.push_back(0xBEEFu);
  disabled_ciphers.push_back(0xDEADu);
  initial_config.disabled_cipher_suites = disabled_ciphers;
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  // Ensure that changing a disabled cipher suite, while still maintaining
  // sorted order, triggers an update.
  disabled_ciphers[1] = 0xCAFEu;
  initial_config.disabled_cipher_suites = disabled_ciphers;
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  // Ensure that removing a disabled cipher suite, while still keeping some
  // cipher suites disabled, triggers an update.
  disabled_ciphers.pop_back();
  initial_config.disabled_cipher_suites = disabled_ciphers;
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  // Test that changing the named groups config triggers an update.
  initial_config.supported_named_groups.pop_back();
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  mock_service.RemoveObserver(&observer);
}

TEST(SSLContextConfigTest, GetSupportedGroups) {
  SSLContextConfig config;

  // Verify the defaults.
  std::vector<uint16_t> expected_supported_groups = {
      SSL_GROUP_X25519_MLKEM768, SSL_GROUP_X25519, SSL_GROUP_SECP256R1,
      SSL_GROUP_SECP384R1};
  std::vector<uint16_t> expected_key_shares = {SSL_GROUP_X25519_MLKEM768,
                                               SSL_GROUP_X25519};

  EXPECT_EQ(config.GetSupportedGroups(), expected_supported_groups);
  EXPECT_EQ(config.GetSupportedGroups(/*key_shares_only=*/true),
            expected_key_shares);

  // Remove the last group, SSL_GROUP_SECP384R1.
  config.supported_named_groups.pop_back();
  // It should be removed from the output of GetSupportedGroups().
  expected_supported_groups.pop_back();
  EXPECT_EQ(config.GetSupportedGroups(), expected_supported_groups);
  // The expected key shares are not changed because the removed group was not
  // configured to send a key share.
  EXPECT_EQ(config.GetSupportedGroups(/*key_shares_only=*/true),
            expected_key_shares);
}

TEST(SSLContextConfigTest, TrustAnchorIDsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kTLSTrustAnchorIDs);

  const std::vector<uint8_t> id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> id2 = {0x02, 0x02};

  SSLContextConfig config;

  EXPECT_FALSE(config.ShouldAdvertiseTrustAnchorIDs());

  config.trust_anchor_ids.insert(id1);
  config.mtc_trust_anchor_ids.push_back(id2);

  EXPECT_FALSE(config.ShouldAdvertiseTrustAnchorIDs());
}

TEST(SSLContextConfigTest, TrustAnchorIDs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTLSTrustAnchorIDs);

  const std::vector<uint8_t> id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> id2 = {0x02, 0x02};
  const std::vector<uint8_t> id3 = {0x13};
  const std::vector<uint8_t> id4 = {0x14, 0x02};
  const std::vector<uint8_t> id5 = {0x25, 0x02};
  const std::vector<uint8_t> id6 = {0x36, 0x09, 0x08};

  SSLContextConfig config;

  EXPECT_FALSE(config.ShouldAdvertiseTrustAnchorIDs());

  config.trust_anchor_ids.insert(id1);
  config.trust_anchor_ids.insert(id2);
  config.trust_anchor_ids.insert(id3);

  EXPECT_TRUE(config.ShouldAdvertiseTrustAnchorIDs());
  EXPECT_EQ(EncodeTrustAnchorIDs({id1}), config.SelectTrustAnchorIDs({id1}));
  EXPECT_EQ(std::vector<uint8_t>(), config.SelectTrustAnchorIDs({id4}));

  scoped_refptr<X509Certificate> leaf =
      net::CertBuilder::CreateSimpleChain(1u)[0]->GetX509Certificate();

  bool used_mtc_fallback = false;

  EXPECT_EQ(EncodeTrustAnchorIDs({id1, id3}),
            config.SelectTrustAnchorIDsForRetry(
                leaf.get(), {id1, id5, id3, id6}, &used_mtc_fallback));
  EXPECT_FALSE(used_mtc_fallback);
  EXPECT_EQ(std::nullopt, config.SelectTrustAnchorIDsForRetry(
                              leaf.get(), {id4, id6}, &used_mtc_fallback));
  EXPECT_FALSE(used_mtc_fallback);
}

TEST(SSLContextConfigTest, TrustAnchorIDsAndMTCs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTLSTrustAnchorIDs);

  const std::vector<uint8_t> known_id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> known_id2 = {0x02, 0x02};
  const std::vector<uint8_t> known_id3 = {0x13};

  const std::vector<uint8_t> other_id1 = {0x14, 0x02};
  const std::vector<uint8_t> other_id2 = {0x25, 0x02};
  const std::vector<uint8_t> other_id3 = {0x36, 0x09, 0x08};

  const std::vector<uint8_t> known_mtc1 = {0x99, 0x02, 0x03};
  const std::vector<uint8_t> newer_mtc1 = {0x99, 0x02, 0x04};

  const std::vector<uint8_t> known_mtc2 = {0x98, 0x05, 0x04};

  const std::vector<uint8_t> known_mtc3 = {0x97, 0x05, 0x03};
  const std::vector<uint8_t> older_mtc3 = {0x97, 0x05, 0x02};

  SSLContextConfig config;

  config.trust_anchor_ids.insert(known_id1);
  config.trust_anchor_ids.insert(known_id2);
  config.trust_anchor_ids.insert(known_id3);
  config.mtc_trust_anchor_ids = {known_mtc1, known_mtc2, known_mtc3};

  EXPECT_TRUE(config.ShouldAdvertiseTrustAnchorIDs());
  // MTCs are included unconditionally, in addition to any intersections with
  // the classic TAIs.
  EXPECT_EQ(
      EncodeTrustAnchorIDs({known_id1, known_mtc1, known_mtc2, known_mtc3}),
      config.SelectTrustAnchorIDs({known_id1}));
  EXPECT_EQ(EncodeTrustAnchorIDs({known_mtc1, known_mtc2, known_mtc3}),
            config.SelectTrustAnchorIDs({other_id1}));
  EXPECT_EQ(EncodeTrustAnchorIDs({known_mtc1, known_mtc2, known_mtc3}),
            config.SelectTrustAnchorIDs({known_mtc1}));

  scoped_refptr<X509Certificate> leaf =
      net::CertBuilder::CreateSimpleChain(1u)[0]->GetX509Certificate();

  // When the server only advertised classic TAIs, no MTC TAI should be
  // advertised in the retry.
  {
    bool used_mtc_fallback = false;
    EXPECT_EQ(EncodeTrustAnchorIDs({known_id1, known_id3}),
              config.SelectTrustAnchorIDsForRetry(
                  leaf.get(), {known_id1, other_id2, known_id3, other_id3},
                  &used_mtc_fallback));
    EXPECT_FALSE(used_mtc_fallback);
  }

  // When there is no intersection between the server TAIs and either the
  // configured classic or MTC TAIs, the retry should not be attempted.
  {
    bool used_mtc_fallback = false;
    EXPECT_EQ(std::nullopt, config.SelectTrustAnchorIDsForRetry(
                                leaf.get(), {other_id1, other_id3, newer_mtc1},
                                &used_mtc_fallback));
    EXPECT_FALSE(used_mtc_fallback);
  }

  // An intersection containing only MTC TAIs.
  {
    bool used_mtc_fallback = false;
    EXPECT_EQ(
        // MTC1 is not included in the retry advertisement, since the server
        // advertised an MTC with a landmark higher than the known one.
        // For MTC2 the server landmark matched the known one, and MTC3 the
        // server landmark was earlier than the known one, so both of those are
        // included.
        EncodeTrustAnchorIDs({known_mtc2, known_mtc3}),
        config.SelectTrustAnchorIDsForRetry(
            leaf.get(),
            {other_id2, newer_mtc1, known_mtc2, older_mtc3, other_id3},
            &used_mtc_fallback));
    EXPECT_FALSE(used_mtc_fallback);
  }

  // An intersection which contains both classic and MTC TAIs.
  {
    bool used_mtc_fallback = false;
    EXPECT_EQ(
        // MTC1 is not included in the retry advertisement, since the server
        // advertised an MTC with a landmark higher than the known one.
        // For MTC2 the server landmark matched the known one, and MTC3 the
        // server landmark was earlier than the known one, so both of those are
        // included.
        EncodeTrustAnchorIDs({known_id1, known_id3, known_mtc2, known_mtc3}),
        config.SelectTrustAnchorIDsForRetry(
            leaf.get(),
            {known_id1, other_id2, known_id3, other_id3, newer_mtc1, known_mtc2,
             older_mtc3},
            &used_mtc_fallback));
    EXPECT_FALSE(used_mtc_fallback);
  }

  // MTC fallback. If the server sent an MTC leaf and it failed, we retry
  // without the MTC TAIs, even if they would have intersected.
  scoped_refptr<X509Certificate> mtc_leaf = GetTestSignaturelessMTC();
  {
    // If there was an intersection with the classic TAIs, retry with that.
    bool used_mtc_fallback = false;
    EXPECT_EQ(EncodeTrustAnchorIDs({known_id2}),
              config.SelectTrustAnchorIDsForRetry(
                  mtc_leaf.get(), {known_id2, other_id1, other_id3, known_mtc2},
                  &used_mtc_fallback));
    EXPECT_TRUE(used_mtc_fallback);
  }
  {
    // If there was no intersection with the classic TAIs, we retry even with
    // an empty TAI list (on the hope that the retry will then be served a
    // default non-MTC cert.)
    bool used_mtc_fallback = false;
    EXPECT_EQ(EncodeTrustAnchorIDs({}),
              config.SelectTrustAnchorIDsForRetry(
                  mtc_leaf.get(), {other_id1, other_id3}, &used_mtc_fallback));
    EXPECT_TRUE(used_mtc_fallback);
  }
}

TEST(SSLContextConfigTest, TrustAnchorIDsRetryMultipleTAIsMatchOneMTCAnchor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTLSTrustAnchorIDs);

  const std::vector<uint8_t> known_mtc1 = {0x99, 0x02, 0x03};
  const std::vector<uint8_t> older_mtc1 = {0x99, 0x02, 0x02};
  const std::vector<uint8_t> newer_mtc1 = {0x99, 0x02, 0x04};

  const std::vector<uint8_t> known_mtc2 = {0x98, 0x05, 0x04};
  const std::vector<uint8_t> newer_mtc2 = {0x98, 0x05, 0x05};

  const std::vector<uint8_t> known_mtc3 = {0x97, 0x05, 0x03};

  SSLContextConfig config;
  config.mtc_trust_anchor_ids = {known_mtc1, known_mtc2, known_mtc3};

  scoped_refptr<X509Certificate> leaf =
      net::CertBuilder::CreateSimpleChain(1u)[0]->GetX509Certificate();

  bool used_mtc_fallback = false;
  // Multiple TAIs advertised by the server matched mtc1, but it should only
  // be included once in the retry list.
  EXPECT_EQ(EncodeTrustAnchorIDs({known_mtc1, known_mtc3}),
            config.SelectTrustAnchorIDsForRetry(
                leaf.get(),
                {older_mtc1, known_mtc1, newer_mtc1, newer_mtc2, known_mtc3},
                &used_mtc_fallback));
  EXPECT_FALSE(used_mtc_fallback);
}

}  // namespace net
