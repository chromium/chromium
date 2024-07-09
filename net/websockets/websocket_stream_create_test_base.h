// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_STREAM_CREATE_TEST_BASE_H_
#define NET_WEBSOCKETS_WEBSOCKET_STREAM_CREATE_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/ssl_info.h"
#include "net/storage_access_api/status.h"
#include "net/test/test_with_task_environment.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_test_util.h"

class GURL;

namespace base {
class OneShotTimer;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace net {

class HttpRequestHeaders;
class HttpResponseHeaders;
class IsolationInfo;
class SiteForCookies;
class URLRequest;
class WebSocketStream;
class WebSocketStreamRequest;
struct WebSocketHandshakeRequestInfo;
struct WebSocketHandshakeResponseInfo;

class WebSocketStreamCreateTestBase : public WithTaskEnvironment {
 public:
  using HeaderKeyValuePair = std::pair<std::string, std::string>;

  WebSocketStreamCreateTestBase();

  WebSocketStreamCreateTestBase(const WebSocketStreamCreateTestBase&) = delete;
  WebSocketStreamCreateTestBase& operator=(
      const WebSocketStreamCreateTestBase&) = delete;

  virtual ~WebSocketStreamCreateTestBase();

  // A wrapper for CreateAndConnectStreamForTesting that knows about our default
  // parameters.
  void CreateAndConnectStream(const GURL& socket_url,
                              const std::vector<std::string>& sub_protocols,
                              const url::Origin& origin,
                              const SiteForCookies& site_for_cookies,
                              StorageAccessApiStatus storage_access_api_status,
                              const IsolationInfo& isolation_info,
                              const HttpRequestHeaders& additional_headers,
                              std::unique_ptr<base::OneShotTimer> timer);

  static std::vector<HeaderKeyValuePair> RequestHeadersToVector(
      const HttpRequestHeaders& headers);
  static std::vector<HeaderKeyValuePair> ResponseHeadersToVector(
      const HttpResponseHeaders& headers);

  const std::string& failure_message() const { return failure_message_; }
  int failure_response_code() const { return failure_response_code_; }
  bool has_failed() const { return has_failed_; }

  // Runs |connect_run_loop_|. It will stop when the connection establishes or
  // fails.
  void WaitUntilConnectDone();

  // Runs |run_loop_waiting_for_on_auth_required_| until OnAuthRequired() is
  // called.
  void WaitUntilOnAuthRequired();

  // A simple function to make the tests more readable.
  std::vector<std::string> NoSubProtocols();

 protected:
  WebSocketTestURLRequestContextHost url_request_context_host_;
  std::unique_ptr<WebSocketStreamRequest> stream_request_;
  // Only set if the connection succeeded.
  std::unique_ptr<WebSocketStream> stream_;
  // Only set if the connection failed.
  std::string failure_message_;
  int failure_response_code_ = -1;
  bool has_failed_ = false;
  std::unique_ptr<WebSocketHandshakeRequestInfo> request_info_;
  std::unique_ptr<WebSocketHandshakeResponseInfo> response_info_;
  std::unique_ptr<WebSocketEventInterface::SSLErrorCallbacks>
      ssl_error_callbacks_;
  SSLInfo ssl_info_;
  bool ssl_fatal_ = false;
  raw_ptr<URLRequest, AcrossTasksDanglingUntriaged> url_request_ = nullptr;
  AuthChallengeInfo auth_challenge_info_;
  base::OnceCallback<void(const AuthCredentials*)> on_auth_required_callback_;

  // This value will be copied to |*credentials| on OnAuthRequired.
  std::optional<AuthCredentials> auth_credentials_;
  // OnAuthRequired returns this value.
  int on_auth_required_rv_ = OK;

  base::RunLoop connect_run_loop_;

  base::RunLoop run_loop_waiting_for_on_auth_required_;

 private:
  class TestConnectDelegate;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_STREAM_CREATE_TEST_BASE_H_
