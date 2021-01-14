// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/test_dns_config_service.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/optional.h"

namespace net {

TestDnsConfigService::TestDnsConfigService()
    : DnsConfigService(base::FilePath::StringPieceType() /* hosts_file_path */,
                       base::nullopt /* config_change_delay */) {}

TestDnsConfigService::~TestDnsConfigService() = default;

bool TestDnsConfigService::StartWatching() {
  return true;
}

void TestDnsConfigService::RefreshConfig() {
  DCHECK(config_for_refresh_);
  InvalidateConfig();
  InvalidateHosts();
  OnConfigRead(config_for_refresh_.value());
  OnHostsRead(config_for_refresh_.value().hosts);
  config_for_refresh_ = base::nullopt;
}

}  // namespace net
