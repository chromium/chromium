// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/proxy/proxy_config.h"

namespace web {

ProxyRule::ProxyRule() = default;
ProxyRule::ProxyRule(const ProxyRule&) = default;
ProxyRule& ProxyRule::operator=(const ProxyRule&) = default;
ProxyRule::ProxyRule(ProxyRule&&) = default;
ProxyRule& ProxyRule::operator=(ProxyRule&&) = default;
ProxyRule::~ProxyRule() = default;

}  // namespace web
