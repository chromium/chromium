// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/test_ssl_config_service.h"

namespace net {

TestSSLConfigService::TestSSLConfigService(const SSLContextConfig& config)
    : config_(config) {}

TestSSLConfigService::~TestSSLConfigService() = default;

SSLContextConfig TestSSLConfigService::GetSSLContextConfig() {
  return config_;
}

bool TestSSLConfigService::CanShareConnectionWithClientCerts(
    const std::string& hostname) const {
  return false;
}

void TestSSLConfigService::UpdateSSLConfigAndNotify(
    const SSLContextConfig& config) {
  config_ = config;
  NotifySSLContextConfigChange();
}

bool TestSSLConfigService::ShouldSuppressLegacyTLSWarning(
    const std::string& hostname) const {
  return false;
}

}  // namespace net
