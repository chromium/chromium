// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config_service_fixed.h"

namespace net {

ProxyConfigServiceFixed::ProxyConfigServiceFixed(
    const ProxyConfigWithAnnotation& pc)
    : pc_(pc) {}

ProxyConfigServiceFixed::~ProxyConfigServiceFixed() = default;

ProxyConfigService::ConfigAvailability
ProxyConfigServiceFixed::GetLatestProxyConfig(
    ProxyConfigWithAnnotation* config) {
  *config = pc_;
  return CONFIG_VALID;
}

}  // namespace net
