// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_fuchsia.h"

#include <memory>

#include "base/files/file_path.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_hosts.h"

namespace net {
namespace internal {

DnsConfigServiceFuchsia::DnsConfigServiceFuchsia()
    : DnsConfigService(
          base::FilePath::StringPieceType() /* hosts_file_path */) {}
DnsConfigServiceFuchsia::~DnsConfigServiceFuchsia() = default;

void DnsConfigServiceFuchsia::ReadConfigNow() {
  // TODO(crbug.com/42050635): Implement this method.
}

void DnsConfigServiceFuchsia::ReadHostsNow() {
  // TODO(crbug.com/42050635): Implement this method.
}

bool DnsConfigServiceFuchsia::StartWatching() {
  // TODO(crbug.com/42050635): Implement this method.
  return false;
}

}  // namespace internal

// static
std::unique_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return std::make_unique<internal::DnsConfigServiceFuchsia>();
}

}  // namespace net
