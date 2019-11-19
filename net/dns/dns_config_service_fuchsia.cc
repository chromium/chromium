// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_fuchsia.h"

#include <memory>

#include "net/dns/dns_config.h"
#include "net/dns/dns_hosts.h"

namespace net {
namespace internal {

DnsConfigServiceFuchsia::DnsConfigServiceFuchsia() = default;
DnsConfigServiceFuchsia::~DnsConfigServiceFuchsia() = default;

void DnsConfigServiceFuchsia::ReadNow() {
  // TODO(crbug.com/950717): Implement this method.
}

bool DnsConfigServiceFuchsia::StartWatching() {
  // TODO(crbug.com/950717): Implement this method.
  return false;
}

}  // namespace internal

// static
std::unique_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return std::make_unique<internal::DnsConfigServiceFuchsia>();
}

}  // namespace net
