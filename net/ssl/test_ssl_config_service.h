// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_TEST_SSL_CONFIG_SERVICE_H_
#define NET_SSL_TEST_SSL_CONFIG_SERVICE_H_

#include <memory>

#include "net/ssl/ech_mode_getter.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class TestSSLConfigService : public SSLConfigService {
 public:
  explicit TestSSLConfigService(const SSLContextConfig& config);
  ~TestSSLConfigService() override;

  void UpdateSSLConfigAndNotify(const SSLContextConfig& config);

  SSLContextConfig GetSSLContextConfig() override;

  EchMode GetEchMode(std::string_view hostname) const override;

  void SetEchModeGetter(std::unique_ptr<EchModeGetter> ech_mode_getter) {
    ech_mode_getter_ = std::move(ech_mode_getter);
  }

  bool CanShareConnectionWithClientCerts(
      std::string_view hostname) const override;

 private:
  SSLContextConfig config_;
  std::unique_ptr<EchModeGetter> ech_mode_getter_;
};

}  // namespace net

#endif  // NET_SSL_TEST_SSL_CONFIG_SERVICE_H_
