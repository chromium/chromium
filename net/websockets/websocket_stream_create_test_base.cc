// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_stream_create_test_base.h"

#include <stddef.h>

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_with_source.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "net/websockets/websocket_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace url {
class Origin;
}  // namespace url

namespace net {
class IPEndPoint;
class SiteForCookies;

using HeaderKeyValuePair = WebSocketStreamCreateTestBase::HeaderKeyValuePair;

class WebSocketStreamCreateTestBase::TestConnectDelegate
    : public WebSocketStream::ConnectDelegate {
 public:
  TestConnectDelegate(WebSocketStreamCreateTestBase* owner,
                      base::OnceClosure done_callback)
      : owner_(owner), done_callback_(std::move(done_callback)) {}

  TestConnectDelegate(const TestConnectDelegate&) = delete;
  TestConnectDelegate& operator=(const TestConnectDelegate&) = delete;

  void OnCreateRequest(URLRequest* request) override {
    owner_->url_request_ = request;
  }

  void OnURLRequestConnected(URLRequest* request,
                             const TransportInfo& info) override {}

  void OnSuccess(
      std::unique_ptr<WebSocketStream> stream,
      std::unique_ptr<WebSocketHandshakeResponseInfo> response) override {
    if (owner_->response_info_)
      ADD_FAILURE();
    owner_->response_info_ = std::move(response);
    stream.swap(owner_->stream_);
    std::move(done_callback_).Run();
  }

  void OnFailure(const std::string& message,
                 int net_error,
                 std::optional<int> response_code) override {
    owner_->has_failed_ = true;
    owner_->failure_message_ = message;
    owner_->failure_response_code_ = response_code.value_or(-1);
    std::move(done_callback_).Run();
  }

  void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo> request) override {
    // Can be called multiple times (in the case of HTTP auth). Last call
    // wins.
    owner_->request_info_ = std::move(request);
  }

  void OnSSLCertificateError(
      std::unique_ptr<WebSocketEventInterface::SSLErrorCallbacks>
          ssl_error_callbacks,
      int net_error,
      const SSLInfo& ssl_info,
      bool fatal) override {
    owner_->ssl_error_callbacks_ = std::move(ssl_error_callbacks);
    owner_->ssl_info_ = ssl_info;
    owner_->ssl_fatal_ = fatal;
  }

  int OnAuthRequired(const AuthChallengeInfo& auth_info,
                     scoped_refptr<HttpResponseHeaders> response_headers,
                     const IPEndPoint& remote_endpoint,
                     base::OnceCallback<void(const AuthCredentials*)> callback,
                     std::optional<AuthCredentials>* credentials) override {
    owner_->run_loop_waiting_for_on_auth_required_.Quit();
    owner_->auth_challenge_info_ = auth_info;
    *credentials = owner_->auth_credentials_;
    owner_->on_auth_required_callback_ = std::move(callback);
    return owner_->on_auth_required_rv_;
  }

 private:
  raw_ptr<WebSocketStreamCreateTestBase> owner_;
  base::OnceClosure done_callback_;
};

WebSocketStreamCreateTestBase::WebSocketStreamCreateTestBase() = default;

WebSocketStreamCreateTestBase::~WebSocketStreamCreateTestBase() = default;

void WebSocketStreamCreateTestBase::CreateAndConnectStream(
    const GURL& socket_url,
    const std::vector<std::string>& sub_protocols,
    const url::Origin& origin,
    const SiteForCookies& site_for_cookies,
    StorageAccessApiStatus storage_access_api_status,
    const IsolationInfo& isolation_info,
    const HttpRequestHeaders& additional_headers,
    std::unique_ptr<base::OneShotTimer> timer) {
  auto connect_delegate = std::make_unique<TestConnectDelegate>(
      this, connect_run_loop_.QuitClosure());
  auto api_delegate = std::make_unique<TestWebSocketStreamRequestAPI>();
  stream_request_ = WebSocketStream::CreateAndConnectStreamForTesting(
      socket_url, sub_protocols, origin, site_for_cookies,
      storage_access_api_status, isolation_info, additional_headers,
      url_request_context_host_.GetURLRequestContext(), NetLogWithSource(),
      TRAFFIC_ANNOTATION_FOR_TESTS, std::move(connect_delegate),
      timer ? std::move(timer) : std::make_unique<base::OneShotTimer>(),
      std::move(api_delegate));
}

std::vector<HeaderKeyValuePair>
WebSocketStreamCreateTestBase::RequestHeadersToVector(
    const HttpRequestHeaders& headers) {
  HttpRequestHeaders::Iterator it(headers);
  std::vector<HeaderKeyValuePair> result;
  while (it.GetNext())
    result.emplace_back(it.name(), it.value());
  return result;
}

std::vector<HeaderKeyValuePair>
WebSocketStreamCreateTestBase::ResponseHeadersToVector(
    const HttpResponseHeaders& headers) {
  size_t iter = 0;
  std::string name, value;
  std::vector<HeaderKeyValuePair> result;
  while (headers.EnumerateHeaderLines(&iter, &name, &value))
    result.emplace_back(name, value);
  return result;
}

void WebSocketStreamCreateTestBase::WaitUntilConnectDone() {
  connect_run_loop_.Run();
}

void WebSocketStreamCreateTestBase::WaitUntilOnAuthRequired() {
  run_loop_waiting_for_on_auth_required_.Run();
}

std::vector<std::string> WebSocketStreamCreateTestBase::NoSubProtocols() {
  return std::vector<std::string>();
}

}  // namespace net
