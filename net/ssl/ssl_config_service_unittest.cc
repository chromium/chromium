// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include <vector>

#include "base/containers/extend.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cert/x509_util.h"
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

  EchMode GetEchMode(std::string_view hostname) const override {
    return EchMode::kOpportunistic;
  }

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

TEST(SSLContextConfigTest, RequestServerPadding) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kAddTLSServerHandshakePadding);
    SSLContextConfig config;
    EXPECT_EQ(std::nullopt, config.RequestServerPadding());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAddTLSServerHandshakePadding,
        {{"AddTLSServerHandshakePaddingBytes", "128"}});
    SSLContextConfig config;
    EXPECT_EQ(128, config.RequestServerPadding());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAddTLSServerHandshakePadding,
        {{"AddTLSServerHandshakePaddingBytes", "0"}});
    SSLContextConfig config;
    EXPECT_EQ(0, config.RequestServerPadding());
  }
}

TEST(SSLContextConfigTest, SelectTrustAnchorIDsUnconditionalAllEnabled) {
  base::test::ScopedFeatureList feature_list;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  feature_list.InitWithFeatures(
      {features::kTLSTrustAnchorIDs, features::kNonMtcTrustAnchorIDs,
       features::kVerifyMTCs},
      {});
#else
  feature_list.InitWithFeatures(
      {features::kTLSTrustAnchorIDs, features::kNonMtcTrustAnchorIDs}, {});
#endif

  const std::vector<uint8_t> id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> id2 = {0x02, 0x02};
  const std::vector<uint8_t> id3 = {0x13};
  const std::vector<uint8_t> mtc1 = {0x99, 0x02, 0x03};

  SSLContextConfig config;
  config.trust_anchor_ids.insert(id1);
  config.trust_anchor_ids.insert(id2);
  config.trust_anchor_ids.insert(id3);
  config.mtc_trust_anchor_ids = {mtc1};

  EXPECT_TRUE(config.ShouldAdvertiseTrustAnchorIDs());
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  EXPECT_THAT(
      x509_util::ParseTlsTrustAnchorIDs(config.SelectAllTrustAnchorIDs()),
      testing::UnorderedElementsAre(id1, id2, id3, mtc1));
#else
  EXPECT_THAT(
      x509_util::ParseTlsTrustAnchorIDs(config.SelectAllTrustAnchorIDs()),
      testing::UnorderedElementsAre(id1, id2, id3));
#endif
}

TEST(SSLContextConfigTest, SelectTrustAnchorIDsUnconditionalNonMtcOnly) {
  base::test::ScopedFeatureList feature_list;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  feature_list.InitWithFeatures(
      {features::kTLSTrustAnchorIDs, features::kNonMtcTrustAnchorIDs},
      {features::kVerifyMTCs});
#else
  feature_list.InitWithFeatures(
      {features::kTLSTrustAnchorIDs, features::kNonMtcTrustAnchorIDs}, {});
#endif

  const std::vector<uint8_t> id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> id2 = {0x02, 0x02};
  const std::vector<uint8_t> id3 = {0x13};
  const std::vector<uint8_t> mtc1 = {0x99, 0x02, 0x03};

  SSLContextConfig config;
  config.trust_anchor_ids.insert(id1);
  config.trust_anchor_ids.insert(id2);
  config.trust_anchor_ids.insert(id3);
  config.mtc_trust_anchor_ids = {mtc1};

  EXPECT_TRUE(config.ShouldAdvertiseTrustAnchorIDs());
  EXPECT_THAT(
      x509_util::ParseTlsTrustAnchorIDs(config.SelectAllTrustAnchorIDs()),
      testing::UnorderedElementsAre(id1, id2, id3));
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
TEST(SSLContextConfigTest, SelectTrustAnchorIDsUnconditionalMtcOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kTLSTrustAnchorIDs, features::kVerifyMTCs},
      {features::kNonMtcTrustAnchorIDs});

  const std::vector<uint8_t> id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> id2 = {0x02, 0x02};
  const std::vector<uint8_t> id3 = {0x13};
  const std::vector<uint8_t> mtc1 = {0x99, 0x02, 0x03};

  SSLContextConfig config;
  config.trust_anchor_ids.insert(id1);
  config.trust_anchor_ids.insert(id2);
  config.trust_anchor_ids.insert(id3);
  config.mtc_trust_anchor_ids = {mtc1};

  EXPECT_TRUE(config.ShouldAdvertiseTrustAnchorIDs());
  EXPECT_THAT(
      x509_util::ParseTlsTrustAnchorIDs(config.SelectAllTrustAnchorIDs()),
      testing::UnorderedElementsAre(mtc1));
}
#endif

TEST(SSLContextConfigTest, SelectTrustAnchorIDsUnconditionalAllDisabled) {
  base::test::ScopedFeatureList feature_list;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  feature_list.InitWithFeatures(
      {features::kTLSTrustAnchorIDs},
      {features::kNonMtcTrustAnchorIDs, features::kVerifyMTCs});
#else
  feature_list.InitWithFeatures({features::kTLSTrustAnchorIDs},
                                {features::kNonMtcTrustAnchorIDs});
#endif

  const std::vector<uint8_t> id1 = {0x01, 0x02, 0x03};
  const std::vector<uint8_t> id2 = {0x02, 0x02};
  const std::vector<uint8_t> id3 = {0x13};
  const std::vector<uint8_t> mtc1 = {0x99, 0x02, 0x03};

  SSLContextConfig config;
  config.trust_anchor_ids.insert(id1);
  config.trust_anchor_ids.insert(id2);
  config.trust_anchor_ids.insert(id3);
  config.mtc_trust_anchor_ids = {mtc1};

  EXPECT_FALSE(config.ShouldAdvertiseTrustAnchorIDs());
  EXPECT_TRUE(config.SelectAllTrustAnchorIDs().empty());
}

}  // namespace net
