// Copyright 2019 The Chromium Authors
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
    std::string_view hostname) const {
  return false;
}

void TestSSLConfigService::UpdateSSLConfigAndNotify(
    const SSLContextConfig& config) {
  config_ = config;
  NotifySSLContextConfigChange();
}

}  // namespace net
