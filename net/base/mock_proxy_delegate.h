// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MOCK_PROXY_DELEGATE_H_
#define NET_BASE_MOCK_PROXY_DELEGATE_H_

#include <string>

#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_request_headers.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace net {

class HttpRequestHeaders;
class HttpResponseHeaders;
class ProxyInfo;
class ProxyResolutionService;

class MockProxyDelegate : public ProxyDelegate {
 public:
  MockProxyDelegate();
  ~MockProxyDelegate() override;
  MOCK_METHOD(void,
              OnResolveProxy,
              (const GURL& url,
               const NetworkAnonymizationKey& network_anonymization_key,
               const std::string& method,
               const ProxyRetryInfoMap& proxy_retry_info,
               ProxyInfo* result),
              (override));
  MOCK_METHOD(std::optional<bool>,
              CanFalloverToNextProxyOverride,
              (const ProxyChain& proxy_chain, int net_error),
              (override));
  MOCK_METHOD(void,
              OnFallback,
              (const ProxyChain& bad_chain, int net_error),
              (override));
  MOCK_METHOD(void,
              OnSuccessfulRequestAfterFailures,
              (const ProxyRetryInfoMap& proxy_retry_info),
              (override));
  MOCK_METHOD((base::expected<HttpRequestHeaders, Error>),
              OnBeforeTunnelRequest,
              (const ProxyChain& proxy_chain,
               size_t proxy_index,
               OnBeforeTunnelRequestCallback callback),
              (override));
  MOCK_METHOD(Error,
              OnTunnelHeadersReceived,
              (const ProxyChain& proxy_chain,
               size_t proxy_index,
               const HttpResponseHeaders& response_headers,
               CompletionOnceCallback callback),
              (override));
  MOCK_METHOD(void,
              SetProxyResolutionService,
              (ProxyResolutionService * proxy_resolution_service),
              (override));
  MOCK_METHOD(bool,
              AliasRequiresProxyOverride,
              (const std::string scheme,
               const std::vector<std::string>& dns_aliases,
               const net::NetworkAnonymizationKey& network_anonymization_key),
              (override));
};

}  // namespace net

#endif  // NET_BASE_MOCK_PROXY_DELEGATE_H_
