// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_TEST_SSL_CONFIG_SERVICE_H_
#define NET_SSL_TEST_SSL_CONFIG_SERVICE_H_

#include "net/ssl/ssl_config_service.h"

namespace net {

class TestSSLConfigService : public SSLConfigService {
 public:
  explicit TestSSLConfigService(const SSLContextConfig& config);
  ~TestSSLConfigService() override;

  void UpdateSSLConfigAndNotify(const SSLContextConfig& config);

  SSLContextConfig GetSSLContextConfig() override;
  bool CanShareConnectionWithClientCerts(
      const std::string& hostname) const override;

 private:
  SSLContextConfig config_;
};

}  // namespace net

#endif  // NET_SSL_TEST_SSL_CONFIG_SERVICE_H_
