// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_FIXED_H_
#define NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_FIXED_H_

#include "base/compiler_specific.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"

namespace net {

// Implementation of ProxyConfigService that returns a fixed result.
class NET_EXPORT ProxyConfigServiceFixed : public ProxyConfigService {
 public:
  explicit ProxyConfigServiceFixed(const ProxyConfigWithAnnotation& pc);
  ~ProxyConfigServiceFixed() override;

  // ProxyConfigService methods:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* config) override;

 private:
  ProxyConfigWithAnnotation pc_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_CONFIG_SERVICE_FIXED_H_
