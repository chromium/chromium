// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      const std::string& hostname) const override {
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
  initial_config.version_min = SSL_PROTOCOL_VERSION_TLS1;
  initial_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;

  MockSSLConfigService mock_service(initial_config);
  MockSSLConfigServiceObserver observer;
  mock_service.AddObserver(&observer);

  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(0);
  mock_service.SetSSLContextConfig(initial_config);

  mock_service.RemoveObserver(&observer);
}

TEST(SSLConfigServiceTest, ForceNotificationNotifiesObservers) {
  SSLContextConfig initial_config;
  initial_config.version_min = SSL_PROTOCOL_VERSION_TLS1;
  initial_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;

  MockSSLConfigService mock_service(initial_config);
  MockSSLConfigServiceObserver observer;
  mock_service.AddObserver(&observer);

  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.ProcessConfigUpdate(initial_config, initial_config, true);

  mock_service.RemoveObserver(&observer);
}

TEST(SSLConfigServiceTest, ConfigUpdatesNotifyObservers) {
  SSLContextConfig initial_config;
  initial_config.version_min = SSL_PROTOCOL_VERSION_TLS1;
  initial_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;

  MockSSLConfigService mock_service(initial_config);
  MockSSLConfigServiceObserver observer;
  mock_service.AddObserver(&observer);

  // Test that changing the SSL version range triggers updates.
  initial_config.version_min = SSL_PROTOCOL_VERSION_TLS1_1;
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  initial_config.version_max = SSL_PROTOCOL_VERSION_TLS1_1;
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

  // Ensure that toggling TLS 1.3 hardening triggers an update.
  initial_config.tls13_hardening_for_local_anchors_enabled = true;
  EXPECT_CALL(observer, OnSSLContextConfigChanged()).Times(1);
  mock_service.SetSSLContextConfig(initial_config);

  mock_service.RemoveObserver(&observer);
}

}  // namespace net
