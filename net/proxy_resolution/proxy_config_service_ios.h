// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_IOS_H_
#define NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_IOS_H_

#include "net/proxy_resolution/polling_proxy_config_service.h"

namespace net {

class ProxyConfigServiceIOS : public PollingProxyConfigService {
 public:
  // Constructs a ProxyConfigService that watches the iOS system proxy settings.
  explicit ProxyConfigServiceIOS(
      const NetworkTrafficAnnotationTag& traffic_annotation);

  ProxyConfigServiceIOS(const ProxyConfigServiceIOS&) = delete;
  ProxyConfigServiceIOS& operator=(const ProxyConfigServiceIOS&) = delete;

  ~ProxyConfigServiceIOS() override;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_IOS_H_
