// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_stream.h"

#include <optional>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/auth.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/websocket_handshake_userdata_key.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "net/websockets/websocket_handshake_stream_create_helper.h"
#include "net/websockets/websocket_http2_handshake_stream.h"
#include "net/websockets/websocket_http3_handshake_stream.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class SSLCertRequestInfo;
class SSLInfo;
class SiteForCookies;

namespace {

// The timeout duration of WebSocket handshake.
// It is defined as the same value as the TCP connection timeout value in
// net/socket/websocket_transport_client_socket_pool.cc to make it hard for
// JavaScript programs to recognize the timeout cause.
constexpr int kHandshakeTimeoutIntervalInSeconds = 240;

class WebSocketStreamRequestImpl;

class Delegate : public URLRequest::Delegate {
 public:
  explicit Delegate(WebSocketStreamRequestImpl* owner) : owner_(owner) {}
  ~Delegate() override = default;

  // Implementation of URLRequest::Delegate methods.
  int OnConnected(URLRequest* request,
                  const TransportInfo& info,
                  CompletionOnceCallback callback) override;

  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override;

  void OnResponseStarted(URLRequest* request, int net_error) override;

  void OnAuthRequired(URLRequest* request,
                      const AuthChallengeInfo& auth_info) override;

  void OnCertificateRequested(URLRequest* request,
                              SSLCertRequestInfo* cert_request_info) override;

  void OnSSLCertificateError(URLRequest* request,
                             int net_error,
                             const SSLInfo& ssl_info,
                             bool fatal) override;

  void OnReadCompleted(URLRequest* request, int bytes_read) override;

 private:
  void OnAuthRequiredComplete(URLRequest* request,
                              const AuthCredentials* auth_credentials);

  raw_ptr<WebSocketStreamRequestImpl> owner_;
};

class WebSocketStreamRequestImpl : public WebSocketStreamRequestAPI {
 public:
  WebSocketStreamRequestImpl(
      const GURL& url,
      const std::vector<std::string>& requested_subprotocols,
      const URLRequestContext* context,
      const url::Origin& origin,
      const SiteForCookies& site_for_cookies,
      StorageAccessApiStatus storage_access_api_status,
      const IsolationInfo& isolation_info,
      const HttpRequestHeaders& additional_headers,
      NetworkTrafficAnnotationTag traffic_annotation,
      std::unique_ptr<WebSocketStream::ConnectDelegate> connect_delegate,
      std::unique_ptr<WebSocketStreamRequestAPI> api_delegate)
      : delegate_(this),
        connect_delegate_(std::move(connect_delegate)),
        url_request_(context->CreateRequest(url,
                                            DEFAULT_PRIORITY,
                                            &delegate_,
                                            traffic_annotation,
                                            /*is_for_websockets=*/true)),
        api_delegate_(std::move(api_delegate)) {
    DCHECK_EQ(IsolationInfo::RequestType::kOther,
              isolation_info.request_type());

    HttpRequestHeaders headers = additional_headers;
    headers.SetHeader(websockets::kUpgrade, websockets::kWebSocketLowercase);
    headers.SetHeader(HttpRequestHeaders::kConnection, websockets::kUpgrade);
    headers.SetHeader(HttpRequestHeaders::kOrigin, origin.Serialize());
    headers.SetHeader(websockets::kSecWebSocketVersion,
                      websockets::kSupportedVersion);

    // Remove HTTP headers that are important to websocket connections: they
    // will be added later.
    headers.RemoveHeader(websockets::kSecWebSocketExtensions);
    headers.RemoveHeader(websockets::kSecWebSocketKey);
    headers.RemoveHeader(websockets::kSecWebSocketProtocol);

    url_request_->SetExtraRequestHeaders(headers);
    url_request_->set_initiator(origin);
    url_request_->set_site_for_cookies(site_for_cookies);
    url_request_->set_isolation_info(isolation_info);

    cookie_util::AddOrRemoveStorageAccessApiOverride(
        url, storage_access_api_status, url_request_->initiator(),
        url_request_->cookie_setting_overrides());

    auto create_helper = std::make_unique<WebSocketHandshakeStreamCreateHelper>(
        connect_delegate_.get(), requested_subprotocols, this);
    url_request_->SetUserData(kWebSocketHandshakeUserDataKey,
                              std::move(create_helper));
    url_request_->SetLoadFlags(LOAD_DISABLE_CACHE | LOAD_BYPASS_CACHE);
    connect_delegate_->OnCreateRequest(url_request_.get());
  }

  // Destroying this object destroys the URLRequest, which cancels the request
  // and so terminates the handshake if it is incomplete.
  ~WebSocketStreamRequestImpl() override {
    if (ws_upgrade_success_) {
      CHECK(url_request_);
      // "Cancel" the request with an error code indicating the upgrade
      // succeeded.
      url_request_->CancelWithError(ERR_WS_UPGRADE);
    }
  }

  void OnBasicHandshakeStreamCreated(
      WebSocketBasicHandshakeStream* handshake_stream) override {
    if (api_delegate_) {
      api_delegate_->OnBasicHandshakeStreamCreated(handshake_stream);
    }
    OnHandshakeStreamCreated(handshake_stream);
  }

  void OnHttp2HandshakeStreamCreated(
      WebSocketHttp2HandshakeStream* handshake_stream) override {
    if (api_delegate_) {
      api_delegate_->OnHttp2HandshakeStreamCreated(handshake_stream);
    }
    OnHandshakeStreamCreated(handshake_stream);
  }

  void OnHttp3HandshakeStreamCreated(
      WebSocketHttp3HandshakeStream* handshake_stream) override {
    if (api_delegate_) {
      api_delegate_->OnHttp3HandshakeStreamCreated(handshake_stream);
    }
    OnHandshakeStreamCreated(handshake_stream);
  }

  void OnFailure(const std::string& message,
                 int net_error,
                 std::optional<int> response_code) override {
    if (api_delegate_)
      api_delegate_->OnFailure(message, net_error, response_code);
    failure_message_ = message;
    failure_net_error_ = net_error;
    failure_response_code_ = response_code;
  }

  void Start(std::unique_ptr<base::OneShotTimer> timer) {
    DCHECK(timer);
    base::TimeDelta timeout(base::Seconds(kHandshakeTimeoutIntervalInSeconds));
    timer_ = std::move(timer);
    timer_->Start(FROM_HERE, timeout,
                  base::BindOnce(&WebSocketStreamRequestImpl::OnTimeout,
                                 base::Unretained(this)));
    url_request_->Start();
  }

  void PerformUpgrade() {
    DCHECK(timer_);
    DCHECK(connect_delegate_);

    timer_->Stop();

    if (!handshake_stream_) {
      ReportFailureWithMessage(
          "No handshake stream has been created or handshake stream is already "
          "destroyed.",
          ERR_FAILED, std::nullopt);
      return;
    }

    if (!handshake_stream_->CanReadFromStream()) {
      ReportFailureWithMessage("Handshake stream is not readable.",
                               ERR_CONNECTION_CLOSED, std::nullopt);
      return;
    }

    ws_upgrade_success_ = true;
    WebSocketHandshakeStreamBase* handshake_stream = handshake_stream_.get();
    handshake_stream_.reset();
    auto handshake_response_info =
        std::make_unique<WebSocketHandshakeResponseInfo>(
            url_request_->url(), url_request_->response_headers(),
            url_request_->GetResponseRemoteEndpoint(),
            url_request_->response_time());
    connect_delegate_->OnSuccess(handshake_stream->Upgrade(),
                                 std::move(handshake_response_info));
  }

  std::string FailureMessageFromNetError(int net_error) {
    if (net_error == ERR_TUNNEL_CONNECTION_FAILED) {
      // This error is common and confusing, so special-case it.
      // TODO(ricea): Include the HostPortPair of the selected proxy server in
      // the error message. This is not currently possible because it isn't set
      // in HttpResponseInfo when a ERR_TUNNEL_CONNECTION_FAILED error happens.
      return "Establishing a tunnel via proxy server failed.";
    } else {
      return base::StrCat(
          {"Error in connection establishment: ", ErrorToString(net_error)});
    }
  }

  void ReportFailure(int net_error, std::optional<int> response_code) {
    DCHECK(timer_);
    timer_->Stop();
    if (failure_message_.empty()) {
      switch (net_error) {
        case OK:
        case ERR_IO_PENDING:
          break;
        case ERR_ABORTED:
          failure_message_ = "WebSocket opening handshake was canceled";
          break;
        case ERR_TIMED_OUT:
          failure_message_ = "WebSocket opening handshake timed out";
          break;
        default:
          failure_message_ = FailureMessageFromNetError(net_error);
          break;
      }
    }

    ReportFailureWithMessage(
        failure_message_, failure_net_error_.value_or(net_error),
        failure_response_code_ ? failure_response_code_ : response_code);
  }

  void ReportFailureWithMessage(const std::string& failure_message,
                                int net_error,
                                std::optional<int> response_code) {
    connect_delegate_->OnFailure(failure_message, net_error, response_code);
  }

  WebSocketStream::ConnectDelegate* connect_delegate() const {
    return connect_delegate_.get();
  }

  void OnTimeout() {
    url_request_->CancelWithError(ERR_TIMED_OUT);
  }

 private:
  void OnHandshakeStreamCreated(
      WebSocketHandshakeStreamBase* handshake_stream) {
    DCHECK(handshake_stream);

    handshake_stream_ = handshake_stream->GetWeakPtr();
  }

  // |delegate_| needs to be declared before |url_request_| so that it gets
  // initialised first and destroyed second.
  Delegate delegate_;

  std::unique_ptr<WebSocketStream::ConnectDelegate> connect_delegate_;

  // Deleting the WebSocketStreamRequestImpl object deletes this URLRequest
  // object, cancelling the whole connection. Must be destroyed before
  // `delegate_`, since `url_request_` has a pointer to it, and before
  // `connect_delegate_`, because there may be a pointer to it further down the
  // stack.
  const std::unique_ptr<URLRequest> url_request_;

  // This is owned by the caller of
  // WebsocketHandshakeStreamCreateHelper::CreateBasicStream() or
  // CreateHttp2Stream() or CreateHttp3Stream().  Both the stream and this
  // object will be destroyed during the destruction of the URLRequest object
  // associated with the handshake. This is only guaranteed to be a valid
  // pointer if the handshake succeeded.
  base::WeakPtr<WebSocketHandshakeStreamBase> handshake_stream_;

  // The failure information supplied by WebSocketBasicHandshakeStream, if any.
  std::string failure_message_;
  std::optional<int> failure_net_error_;
  std::optional<int> failure_response_code_;

  // A timer for handshake timeout.
  std::unique_ptr<base::OneShotTimer> timer_;

  // Set to true if the websocket upgrade succeeded.
  bool ws_upgrade_success_ = false;

  // A delegate for On*HandshakeCreated and OnFailure calls.
  std::unique_ptr<WebSocketStreamRequestAPI> api_delegate_;
};

class SSLErrorCallbacks : public WebSocketEventInterface::SSLErrorCallbacks {
 public:
  explicit SSLErrorCallbacks(URLRequest* url_request)
      : url_request_(url_request->GetWeakPtr()) {}

  void CancelSSLRequest(int error, const SSLInfo* ssl_info) override {
    if (!url_request_)
      return;

    if (ssl_info) {
      url_request_->CancelWithSSLError(error, *ssl_info);
    } else {
      url_request_->CancelWithError(error);
    }
  }

  void ContinueSSLRequest() override {
    if (url_request_)
      url_request_->ContinueDespiteLastError();
  }

 private:
  base::WeakPtr<URLRequest> url_request_;
};

int Delegate::OnConnected(URLRequest* request,
                          const TransportInfo& info,
                          CompletionOnceCallback callback) {
  owner_->connect_delegate()->OnURLRequestConnected(request, info);
  return OK;
}

void Delegate::OnReceivedRedirect(URLRequest* request,
                                  const RedirectInfo& redirect_info,
                                  bool* defer_redirect) {
  // This code should never be reached for externally generated redirects,
  // as WebSocketBasicHandshakeStream is responsible for filtering out
  // all response codes besides 101, 401, and 407. As such, the URLRequest
  // should never see a redirect sent over the network. However, internal
  // redirects also result in this method being called, such as those
  // caused by HSTS.
  // Because it's security critical to prevent externally-generated
  // redirects in WebSockets, perform additional checks to ensure this
  // is only internal.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("wss");
  GURL expected_url = request->original_url().ReplaceComponents(replacements);
  if (redirect_info.new_method != "GET" ||
      redirect_info.new_url != expected_url) {
    // This should not happen.
    DLOG(FATAL) << "Unauthorized WebSocket redirect to "
                << redirect_info.new_method << " "
                << redirect_info.new_url.spec();
    request->Cancel();
  }
}

void Delegate::OnResponseStarted(URLRequest* request, int net_error) {
  DCHECK_NE(ERR_IO_PENDING, net_error);

  const bool is_http2 =
      request->response_info().connection_info == HttpConnectionInfo::kHTTP2;

  // All error codes, including OK and ABORTED, as with
  // Net.ErrorCodesForMainFrame4
  base::UmaHistogramSparse("Net.WebSocket.ErrorCodes", -net_error);
  if (is_http2) {
    base::UmaHistogramSparse("Net.WebSocket.ErrorCodes.Http2", -net_error);
  }
  if (net::IsLocalhost(request->url())) {
    base::UmaHistogramSparse("Net.WebSocket.ErrorCodes_Localhost", -net_error);
  } else {
    base::UmaHistogramSparse("Net.WebSocket.ErrorCodes_NotLocalhost",
                             -net_error);
  }

  if (net_error != OK) {
    DVLOG(3) << "OnResponseStarted (request failed)";
    owner_->ReportFailure(net_error, std::nullopt);
    return;
  }
  const int response_code = request->GetResponseCode();
  DVLOG(3) << "OnResponseStarted (response code " << response_code << ")";

  if (is_http2) {
    if (response_code == HTTP_OK) {
      owner_->PerformUpgrade();
      return;
    }

    owner_->ReportFailure(net_error, std::nullopt);
    return;
  }

  switch (response_code) {
    case HTTP_SWITCHING_PROTOCOLS:
      owner_->PerformUpgrade();
      return;

    case HTTP_UNAUTHORIZED:
      owner_->ReportFailureWithMessage(
          "HTTP Authentication failed; no valid credentials available",
          net_error, response_code);
      return;

    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      owner_->ReportFailureWithMessage("Proxy authentication failed", net_error,
                                       response_code);
      return;

    default:
      owner_->ReportFailure(net_error, response_code);
  }
}

void Delegate::OnAuthRequired(URLRequest* request,
                              const AuthChallengeInfo& auth_info) {
  std::optional<AuthCredentials> credentials;
  // This base::Unretained(this) relies on an assumption that |callback| can
  // be called called during the opening handshake.
  int rv = owner_->connect_delegate()->OnAuthRequired(
      auth_info, request->response_headers(),
      request->GetResponseRemoteEndpoint(),
      base::BindOnce(&Delegate::OnAuthRequiredComplete, base::Unretained(this),
                     request),
      &credentials);
  request->LogBlockedBy("WebSocketStream::Delegate::OnAuthRequired");
  if (rv == ERR_IO_PENDING)
    return;
  if (rv != OK) {
    request->LogUnblocked();
    owner_->ReportFailure(rv, std::nullopt);
    return;
  }
  OnAuthRequiredComplete(request, nullptr);
}

void Delegate::OnAuthRequiredComplete(URLRequest* request,
                                      const AuthCredentials* credentials) {
  request->LogUnblocked();
  if (!credentials) {
    request->CancelAuth();
    return;
  }
  request->SetAuth(*credentials);
}

void Delegate::OnCertificateRequested(URLRequest* request,
                                      SSLCertRequestInfo* cert_request_info) {
  // This method is called when a client certificate is requested, and the
  // request context does not already contain a client certificate selection for
  // the endpoint. In this case, a main frame resource request would pop-up UI
  // to permit selection of a client certificate, but since WebSockets are
  // sub-resources they should not pop-up UI and so there is nothing more we can
  // do.
  request->Cancel();
}

void Delegate::OnSSLCertificateError(URLRequest* request,
                                     int net_error,
                                     const SSLInfo& ssl_info,
                                     bool fatal) {
  owner_->connect_delegate()->OnSSLCertificateError(
      std::make_unique<SSLErrorCallbacks>(request), net_error, ssl_info, fatal);
}

void Delegate::OnReadCompleted(URLRequest* request, int bytes_read) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

WebSocketStreamRequest::~WebSocketStreamRequest() = default;

WebSocketStream::WebSocketStream() = default;
WebSocketStream::~WebSocketStream() = default;

WebSocketStream::ConnectDelegate::~ConnectDelegate() = default;

std::unique_ptr<WebSocketStreamRequest> WebSocketStream::CreateAndConnectStream(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const url::Origin& origin,
    const SiteForCookies& site_for_cookies,
    StorageAccessApiStatus storage_access_api_status,
    const IsolationInfo& isolation_info,
    const HttpRequestHeaders& additional_headers,
    URLRequestContext* url_request_context,
    const NetLogWithSource& net_log,
    NetworkTrafficAnnotationTag traffic_annotation,
    std::unique_ptr<ConnectDelegate> connect_delegate) {
  auto request = std::make_unique<WebSocketStreamRequestImpl>(
      socket_url, requested_subprotocols, url_request_context, origin,
      site_for_cookies, storage_access_api_status, isolation_info,
      additional_headers, traffic_annotation, std::move(connect_delegate),
      nullptr);
  request->Start(std::make_unique<base::OneShotTimer>());
  return std::move(request);
}

std::unique_ptr<WebSocketStreamRequest>
WebSocketStream::CreateAndConnectStreamForTesting(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const url::Origin& origin,
    const SiteForCookies& site_for_cookies,
    StorageAccessApiStatus storage_access_api_status,
    const IsolationInfo& isolation_info,
    const HttpRequestHeaders& additional_headers,
    URLRequestContext* url_request_context,
    const NetLogWithSource& net_log,
    NetworkTrafficAnnotationTag traffic_annotation,
    std::unique_ptr<WebSocketStream::ConnectDelegate> connect_delegate,
    std::unique_ptr<base::OneShotTimer> timer,
    std::unique_ptr<WebSocketStreamRequestAPI> api_delegate) {
  auto request = std::make_unique<WebSocketStreamRequestImpl>(
      socket_url, requested_subprotocols, url_request_context, origin,
      site_for_cookies, storage_access_api_status, isolation_info,
      additional_headers, traffic_annotation, std::move(connect_delegate),
      std::move(api_delegate));
  request->Start(std::move(timer));
  return std::move(request);
}

}  // namespace net
