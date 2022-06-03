// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_over_https_server_config.h"

namespace net {

DnsOverHttpsServerConfig::DnsOverHttpsServerConfig(
    const std::string& server_template,
    bool use_post)
    : server_template(server_template), use_post(use_post) {}

bool DnsOverHttpsServerConfig::operator==(
    const DnsOverHttpsServerConfig& other) const {
  return server_template == other.server_template && use_post == other.use_post;
}

}  // namespace net
