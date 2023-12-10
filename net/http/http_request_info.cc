// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/secure_dns_policy.h"

namespace net {

HttpRequestInfo::HttpRequestInfo() = default;

HttpRequestInfo::HttpRequestInfo(const HttpRequestInfo& other) = default;
HttpRequestInfo& HttpRequestInfo::operator=(const HttpRequestInfo& other) =
    default;
HttpRequestInfo::HttpRequestInfo(HttpRequestInfo&& other) = default;
HttpRequestInfo& HttpRequestInfo::operator=(HttpRequestInfo&& other) = default;

HttpRequestInfo::~HttpRequestInfo() = default;

bool HttpRequestInfo::IsConsistent() const {
  return network_anonymization_key ==
         NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
             network_isolation_key);
}

}  // namespace net
