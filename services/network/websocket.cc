// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket.h"

#include <inttypes.h>
#include <string.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cookies/static_cookie_policy.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"  // for WebSocketFrameHeader::OpCode
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_interceptor.h"
#include "services/network/websocket_factory.h"
#include "services/network/websocket_interceptor.h"

namespace network {
namespace {

// What is considered a "small message" for the purposes of small message
// reassembly.
constexpr uint64_t kSmallMessageThreshhold = 1 << 16;

// The capacity of the data pipe to use for received messages, in bytes. Optimal
// value depends on the platform. |2^n - delta| is better than 2^n on Linux. See
// crrev.com/c/1792208.
constexpr uint32_t kReceiveDataPipeCapacity =
    BUILDFLAG(IS_ANDROID) ? 1 << 16 : 131000;

// Convert a mojom::WebSocketMessageType to a
// net::WebSocketFrameHeader::OpCode
net::WebSocketFrameHeader::OpCode MessageTypeToOpCode(
    mojom::WebSocketMessageType type) {
  DCHECK(type == mojom::WebSocketMessageType::CONTINUATION ||
         type == mojom::WebSocketMessageType::TEXT ||
         type == mojom::WebSocketMessageType::BINARY);
  typedef net::WebSocketFrameHeader::OpCode OpCode;
  // These compile asserts verify that the same underlying values are used for
  // both types, so we can simply cast between them.
  static_assert(
      static_cast<OpCode>(mojom::WebSocketMessageType::CONTINUATION) ==
          net::WebSocketFrameHeader::kOpCodeContinuation,
      "enum values must match for opcode continuation");
  static_assert(static_cast<OpCode>(mojom::WebSocketMessageType::TEXT) ==
                    net::WebSocketFrameHeader::kOpCodeText,
                "enum values must match for opcode text");
  static_assert(static_cast<OpCode>(mojom::WebSocketMessageType::BINARY) ==
                    net::WebSocketFrameHeader::kOpCodeBinary,
                "enum values must match for opcode binary");
  return static_cast<OpCode>(type);
}

mojom::WebSocketMessageType OpCodeToMessageType(
    net::WebSocketFrameHeader::OpCode opCode) {
  DCHECK(opCode == net::WebSocketFrameHeader::kOpCodeContinuation ||
         opCode == net::WebSocketFrameHeader::kOpCodeText ||
         opCode == net::WebSocketFrameHeader::kOpCodeBinary);
  // This cast is guaranteed valid by the static_assert() statements above.
  return static_cast<mojom::WebSocketMessageType>(opCode);
}

mojom::WebSocketHandshakeResponsePtr ToMojo(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response,
    bool has_raw_headers_access) {
  mojom::WebSocketHandshakeResponsePtr response_to_pass(
      mojom::WebSocketHandshakeResponse::New());
  response_to_pass->url.Swap(&response->url);
  response_to_pass->status_code = response->headers->response_code();
  response_to_pass->status_text = response->headers->GetStatusText();
  response_to_pass->http_version = response->headers->GetHttpVersion();
  response_to_pass->remote_endpoint = response->remote_endpoint;
  size_t iter = 0;
  std::string name, value;
  std::string headers_text =
      base::StrCat({response->headers->GetStatusLine(), "\r\n"});
  while (response->headers->EnumerateHeaderLines(&iter, &name, &value)) {
    if (has_raw_headers_access ||
        !net::HttpResponseHeaders::IsCookieResponseHeader(name)) {
      // We drop cookie-related headers such as "set-cookie" when the
      // renderer doesn't have access.
      response_to_pass->headers.push_back(mojom::HttpHeader::New(name, value));
      base::StrAppend(&headers_text, {name, ": ", value, "\r\n"});
    }
  }
  headers_text.append("\r\n");
  response_to_pass->headers_text = headers_text;

  return response_to_pass;
}

}  // namespace

// Implementation of net::WebSocketEventInterface. Receives events from our
// WebSocketChannel object.
class WebSocket::WebSocketEventHandler final
    : public net::WebSocketEventInterface {
 public:
  explicit WebSocketEventHandler(WebSocket* impl);

  WebSocketEventHandler(const WebSocketEventHandler&) = delete;
  WebSocketEventHandler& operator=(const WebSocketEventHandler&) = delete;

  ~WebSocketEventHandler() override;

  // net::WebSocketEventInterface implementation

  void OnCreateURLRequest(net::URLRequest* url_request) override;
  void OnURLRequestConnected(net::URLRequest* request,
                             const net::TransportInfo& info) override;
  void OnAddChannelResponse(
      std::unique_ptr<net::WebSocketHandshakeResponseInfo> response,
      const std::string& selected_subprotocol,
      const std::string& extensions) override;
  void OnDataFrame(bool fin,
                   WebSocketMessageType type,
                   base::span<const char> payload) override;
  void OnSendDataFrameDone() override;
  bool HasPendingDataFrames() override;
  void OnClosingHandshake() override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;
  void OnFailChannel(const std::string& message,
                     int net_error,
                     std::optional<int> response_code) override;
  void OnStartOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) override;
  void OnSSLCertificateError(
      std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks>
          callbacks,
      const GURL& url,
      int net_error,
      const net::SSLInfo& ssl_info,
      bool fatal) override;
  int OnAuthRequired(
      const net::AuthChallengeInfo& auth_info,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const net::IPEndPoint& remote_endpoint,
      base::OnceCallback<void(const net::AuthCredentials*)> callback,
      std::optional<net::AuthCredentials>* credentials) override;

 private:
  const raw_ptr<WebSocket> impl_;
};

WebSocket::WebSocketEventHandler::WebSocketEventHandler(WebSocket* impl)
    : impl_(impl) {
  DVLOG(1) << "WebSocketEventHandler created @"
           << reinterpret_cast<void*>(this);
}

WebSocket::WebSocketEventHandler::~WebSocketEventHandler() {
  DVLOG(1) << "WebSocketEventHandler destroyed @"
           << reinterpret_cast<void*>(this);
}

void WebSocket::WebSocketEventHandler::OnCreateURLRequest(
    net::URLRequest* url_request) {
  url_request->SetUserData(WebSocket::kUserDataKey,
                           std::make_unique<UnownedPointer>(impl_));
  if (impl_->throttling_profile_id_) {
    impl_->frame_interceptor_ = std::make_unique<WebSocketInterceptor>(
        url_request->net_log().source().id, impl_->throttling_profile_id_);
  }
}

void WebSocket::WebSocketEventHandler::OnURLRequestConnected(
    net::URLRequest* request,
    const net::TransportInfo& info) {
  if (impl_->url_loader_network_observer_) {
    mojom::IPAddressSpace ip_address_space =
        TransportInfoToIPAddressSpace(info);
    if (ip_address_space == network::mojom::IPAddressSpace::kLocal ||
        ip_address_space == network::mojom::IPAddressSpace::kPrivate) {
      impl_->url_loader_network_observer_->OnWebSocketConnectedToPrivateNetwork(
          ip_address_space);
    }
  }
}

void WebSocket::WebSocketEventHandler::OnAddChannelResponse(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response,
    const std::string& selected_protocol,
    const std::string& extensions) {
  DVLOG(3) << "WebSocketEventHandler::OnAddChannelResponse @"
           << reinterpret_cast<void*>(this) << " selected_protocol=\""
           << selected_protocol << "\""
           << " extensions=\"" << extensions << "\"";

  impl_->handshake_succeeded_ = true;
  if (impl_->pending_connection_tracker_) {
    impl_->pending_connection_tracker_->OnCompleteHandshake();
  }

  const MojoCreateDataPipeOptions data_pipe_options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
      kReceiveDataPipeCapacity};
  mojo::ScopedDataPipeConsumerHandle readable;
  const MojoResult result =
      mojo::CreateDataPipe(&data_pipe_options, impl_->writable_, readable);
  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << "mojo::CreateDataPipe error:" << result;
    impl_->Reset();
    return;
  }
  const MojoResult mojo_result = impl_->writable_watcher_.Watch(
      impl_->writable_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&WebSocket::OnWritable, base::Unretained(impl_)));
  DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle writable;
  const MojoResult write_pipe_result =
      mojo::CreateDataPipe(&data_pipe_options, writable, impl_->readable_);
  if (write_pipe_result != MOJO_RESULT_OK) {
    DVLOG(1) << "mojo::CreateDataPipe error:" << result;
    impl_->Reset();
    return;
  }
  const MojoResult mojo_readable_result = impl_->readable_watcher_.Watch(
      impl_->readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&WebSocket::OnReadable, base::Unretained(impl_)));
  DCHECK_EQ(mojo_readable_result, MOJO_RESULT_OK);

  mojom::WebSocketHandshakeResponsePtr mojo_response =
      ToMojo(std::move(response), !!impl_->has_raw_headers_access_);
  mojo_response->selected_protocol = selected_protocol;
  mojo_response->extensions = extensions;
  impl_->handshake_client_->OnConnectionEstablished(
      impl_->receiver_.BindNewPipeAndPassRemote(),
      impl_->client_.BindNewPipeAndPassReceiver(), std::move(mojo_response),
      std::move(readable), std::move(writable));
  impl_->receiver_.set_disconnect_handler(base::BindOnce(
      &WebSocket::OnConnectionError, base::Unretained(impl_), FROM_HERE));
  impl_->handshake_client_.reset();
  impl_->auth_handler_.reset();
  impl_->header_client_.reset();
  impl_->client_.set_disconnect_handler(base::BindOnce(
      &WebSocket::OnConnectionError, base::Unretained(impl_), FROM_HERE));
}

void WebSocket::WebSocketEventHandler::OnDataFrame(
    bool fin,
    net::WebSocketFrameHeader::OpCode type,
    base::span<const char> payload) {
  DVLOG(3) << "WebSocketEventHandler::OnDataFrame @"
           << reinterpret_cast<void*>(this) << " fin=" << fin
           << " type=" << type << " data is " << payload.size() << " bytes";
  impl_->client_->OnDataFrame(fin, OpCodeToMessageType(type), payload.size());
  if (payload.size() > 0) {
    impl_->pending_data_frames_.push(payload);
  }
  impl_->SendPendingDataFrames(InterruptionReason::kNone);
}

void WebSocket::WebSocketEventHandler::OnSendDataFrameDone() {
  impl_->ResumeDataPipeReading();
  return;
}

bool WebSocket::WebSocketEventHandler::HasPendingDataFrames() {
  return !impl_->pending_data_frames_.empty();
}

void WebSocket::WebSocketEventHandler::OnClosingHandshake() {
  DVLOG(3) << "WebSocketEventHandler::OnClosingHandshake @"
           << reinterpret_cast<void*>(this);

  impl_->client_->OnClosingHandshake();
}

void WebSocket::WebSocketEventHandler::OnDropChannel(
    bool was_clean,
    uint16_t code,
    const std::string& reason) {
  DVLOG(3) << "WebSocketEventHandler::OnDropChannel @"
           << reinterpret_cast<void*>(this) << " was_clean=" << was_clean
           << " code=" << code << " reason=\"" << reason << "\"";

  impl_->client_->OnDropChannel(was_clean, code, reason);
  impl_->Reset();
}

void WebSocket::WebSocketEventHandler::OnFailChannel(
    const std::string& message,
    int net_error,
    std::optional<int> response_code) {
  DVLOG(3) << "WebSocketEventHandler::OnFailChannel @"
           << reinterpret_cast<void*>(this) << " message=\"" << message << "\""
           << " error=" << net_error
           << " response_code=" << response_code.value_or(-1);

  // OnAddChannelResponse may have already reset |impl_->handshake_client_| if
  // the failure happened after a successful connection.
  if (impl_->handshake_client_.is_bound()) {
    impl_->handshake_client_->OnFailure(message, net_error,
                                        response_code.value_or(-1));
    // Additional error information is provided via OnFailure in this case.
    impl_->handshake_client_.reset();
  }
  impl_->client_.ResetWithReason(0, message);
  impl_->Reset();
}

void WebSocket::WebSocketEventHandler::OnStartOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
  DVLOG(3) << "WebSocketEventHandler::OnStartOpeningHandshake @"
           << reinterpret_cast<void*>(this)
           << " can_read_raw_cookies =" << impl_->has_raw_headers_access_;

  mojom::WebSocketHandshakeRequestPtr request_to_pass(
      mojom::WebSocketHandshakeRequest::New());
  request_to_pass->url.Swap(&request->url);
  std::string headers_text = base::StringPrintf(
      "GET %s HTTP/1.1\r\n", request_to_pass->url.spec().c_str());
  net::HttpRequestHeaders::Iterator it(request->headers);
  while (it.GetNext()) {
    if (!impl_->has_raw_headers_access_ &&
        base::EqualsCaseInsensitiveASCII(it.name(),
                                         net::HttpRequestHeaders::kCookie)) {
      continue;
    }
    mojom::HttpHeaderPtr header(mojom::HttpHeader::New());
    header->name = it.name();
    header->value = it.value();
    request_to_pass->headers.push_back(std::move(header));
    headers_text.append(base::StringPrintf("%s: %s\r\n", it.name().c_str(),
                                           it.value().c_str()));
  }
  headers_text.append("\r\n");
  request_to_pass->headers_text = std::move(headers_text);

  impl_->handshake_client_->OnOpeningHandshakeStarted(
      std::move(request_to_pass));
}

void WebSocket::WebSocketEventHandler::OnSSLCertificateError(
    std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks,
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  DVLOG(3) << "WebSocketEventHandler::OnSSLCertificateError"
           << reinterpret_cast<void*>(this) << " url=" << url.spec()
           << " cert_status=" << ssl_info.cert_status << " fatal=" << fatal;
  if (!impl_->url_loader_network_observer_) {
    impl_->OnSSLCertificateErrorResponse(std::move(callbacks), ssl_info,
                                         net::ERR_INSECURE_RESPONSE);
    return;
  }
  impl_->url_loader_network_observer_->OnSSLCertificateError(
      url, net_error, ssl_info, fatal,
      base::BindOnce(&WebSocket::OnSSLCertificateErrorResponse,
                     impl_->weak_ptr_factory_.GetWeakPtr(),
                     std::move(callbacks), ssl_info));
}

int WebSocket::WebSocketEventHandler::OnAuthRequired(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const net::IPEndPoint& remote_endpoint,
    base::OnceCallback<void(const net::AuthCredentials*)> callback,
    std::optional<net::AuthCredentials>* credentials) {
  DVLOG(3) << "WebSocketEventHandler::OnAuthRequired"
           << reinterpret_cast<void*>(this);
  if (!impl_->auth_handler_) {
    *credentials = std::nullopt;
    return net::OK;
  }

  impl_->auth_handler_->OnAuthRequired(
      auth_info, std::move(response_headers), remote_endpoint,
      base::BindOnce(&WebSocket::OnAuthRequiredComplete,
                     impl_->weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
  return net::ERR_IO_PENDING;
}

struct WebSocket::CloseInfo {
  CloseInfo(uint16_t code, const std::string& reason)
      : code(code), reason(reason) {}

  const uint16_t code;
  const std::string reason;
};

WebSocket::WebSocket(
    WebSocketFactory* factory,
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const net::SiteForCookies& site_for_cookies,
    net::StorageAccessApiStatus storage_access_api_status,
    const net::IsolationInfo& isolation_info,
    std::vector<mojom::HttpHeaderPtr> additional_headers,
    const url::Origin& origin,
    uint32_t options,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    HasRawHeadersAccess has_raw_headers_access,
    mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<mojom::WebSocketAuthenticationHandler> auth_handler,
    mojo::PendingRemote<mojom::TrustedHeaderClient> header_client,
    std::optional<WebSocketThrottler::PendingConnection>
        pending_connection_tracker,
    base::TimeDelta delay,
    const std::optional<base::UnguessableToken>& throttling_profile_id)
    : factory_(factory),
      url_loader_network_observer_(std::move(url_loader_network_observer)),
      handshake_client_(std::move(handshake_client)),
      auth_handler_(std::move(auth_handler)),
      header_client_(std::move(header_client)),
      pending_connection_tracker_(std::move(pending_connection_tracker)),
      delay_(delay),
      options_(options),
      traffic_annotation_(traffic_annotation),
      origin_(std::move(origin)),
      site_for_cookies_(site_for_cookies),
      isolation_info_(isolation_info),
      has_raw_headers_access_(has_raw_headers_access),
      writable_watcher_(FROM_HERE,
                        mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                        base::SingleThreadTaskRunner::GetCurrentDefault()),
      readable_watcher_(FROM_HERE,
                        mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                        base::SingleThreadTaskRunner::GetCurrentDefault()),
      throttling_profile_id_(throttling_profile_id) {
  DCHECK(handshake_client_);
  // |delay| should be zero if this connection is not throttled.
  DCHECK(pending_connection_tracker_.has_value() || delay.is_zero());
  if (auth_handler_) {
    // Make sure the request dies if |auth_handler_| has an error, otherwise
    // requests can hang.
    auth_handler_.set_disconnect_handler(base::BindOnce(
        &WebSocket::OnConnectionError, base::Unretained(this), FROM_HERE));
  }
  if (header_client_) {
    // Make sure the request dies if |header_client_| has an error, otherwise
    // requests can hang.
    header_client_.set_disconnect_handler(base::BindOnce(
        &WebSocket::OnConnectionError, base::Unretained(this), FROM_HERE));
  }
  handshake_client_.set_disconnect_handler(base::BindOnce(
      &WebSocket::OnConnectionError, base::Unretained(this), FROM_HERE));
  if (delay_.is_positive()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebSocket::AddChannel, weak_ptr_factory_.GetWeakPtr(),
                       url, requested_protocols, site_for_cookies,
                       storage_access_api_status, isolation_info,
                       std::move(additional_headers)),
        delay_);
    return;
  }
  AddChannel(url, requested_protocols, site_for_cookies,
             storage_access_api_status, isolation_info,
             std::move(additional_headers));
}

WebSocket::~WebSocket() {
  if (channel_ && handshake_succeeded_) {
    StartClosingHandshake(static_cast<uint16_t>(net::kWebSocketErrorGoingAway),
                          "");
  }
}

// static
const void* const WebSocket::kUserDataKey = &WebSocket::kUserDataKey;

void WebSocket::SendMessage(mojom::WebSocketMessageType type,
                            uint64_t data_length) {
  DVLOG(3) << "WebSocket::SendMessage @" << reinterpret_cast<void*>(this)
           << " type=" << type << " data is " << data_length << " bytes";

  DCHECK(channel_) << "WebSocket::SendMessage is called but there is "
                      "no active channel.";
  DCHECK(handshake_succeeded_);

  // This is guaranteed by mojo.
  if (type == mojom::WebSocketMessageType::CONTINUATION) {
    Reset();
    return;
  }
  DCHECK(IsKnownEnumValue(type));

  const bool do_not_fragment = data_length <= kSmallMessageThreshhold;

  pending_send_data_frames_.emplace(type, data_length, do_not_fragment);

  // Safe if ReadAndSendFromDataPipe() deletes |this| because this method is
  // only called from mojo.
  if (!blocked_on_websocket_channel_) {
    ReadAndSendFromDataPipe(InterruptionReason::kNone);
  }
}

void WebSocket::StartReceiving() {
  DCHECK(pending_data_frames_.empty());
  std::ignore = channel_->ReadFrames();
}

void WebSocket::StartClosingHandshake(uint16_t code,
                                      const std::string& reason) {
  DVLOG(3) << "WebSocket::StartClosingHandshake @" << this << " code=" << code
           << " reason=\"" << reason << "\"";

  DCHECK(channel_) << "WebSocket::StartClosingHandshake is called but there is "
                      "no active channel.";
  DCHECK(handshake_succeeded_);
  if (!pending_send_data_frames_.empty()) {
    // This has only been observed happening on Windows 7, but the Mojo API
    // doesn't guarantee that it won't happen on other platforms.
    pending_start_closing_handshake_ =
        std::make_unique<CloseInfo>(code, reason);
    return;
  }
  std::ignore = channel_->StartClosingHandshake(code, reason);
}

bool WebSocket::AllowCookies(const GURL& url) const {
  net::StaticCookiePolicy::Type policy =
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES;
  if (options_ & mojom::kWebSocketOptionBlockAllCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_COOKIES;
  } else if (options_ & mojom::kWebSocketOptionBlockThirdPartyCookies) {
    policy = net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES;
  } else {
    return true;
  }
  return net::StaticCookiePolicy(policy).CanAccessCookies(
             url, site_for_cookies_) == net::OK;
}

bool WebSocket::RevokeIfNonceMatches(const base::UnguessableToken& nonce) {
  if (isolation_info_.nonce() != nonce) {
    return false;
  }

  std::string message =
      "This WebSocket is in a frame whose network access "
      "is being revoked.";
  DVLOG(3) << "WebSocketEventHandler::RevokeIfNonceMatches @"
           << reinterpret_cast<void*>(this) << " " << message;
  // OnAddChannelResponse may have already reset |impl_->handshake_client_| if
  // the failure happened after a successful connection.
  if (handshake_client_.is_bound()) {
    handshake_client_->OnFailure(message, net::kWebSocketErrorGoingAway, -1);
  }
  client_.ResetWithReason(0, message);

  return true;
}

int WebSocket::OnBeforeStartTransaction(
    const net::HttpRequestHeaders& headers,
    net::NetworkDelegate::OnBeforeStartTransactionCallback callback) {
  if (header_client_) {
    header_client_->OnBeforeSendHeaders(
        headers,
        base::BindOnce(&WebSocket::OnBeforeSendHeadersComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

int WebSocket::OnHeadersReceived(
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    std::optional<GURL>* preserve_fragment_on_redirect_url) {
  if (header_client_) {
    header_client_->OnHeadersReceived(
        original_response_headers->raw_headers(), net::IPEndPoint(),
        base::BindOnce(&WebSocket::OnHeadersReceivedComplete,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       override_response_headers,
                       preserve_fragment_on_redirect_url));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

// static
WebSocket* WebSocket::ForRequest(const net::URLRequest& request) {
  auto* pointer =
      static_cast<UnownedPointer*>(request.GetUserData(kUserDataKey));
  if (!pointer)
    return nullptr;
  return pointer->get();
}

void WebSocket::OnConnectionError(const base::Location& set_from) {
  DVLOG(3) << "WebSocket::OnConnectionError @" << reinterpret_cast<void*>(this)
           << ", set_from=" << set_from.ToString();

  factory_->Remove(this);
}

void WebSocket::AddChannel(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const net::SiteForCookies& site_for_cookies,
    net::StorageAccessApiStatus storage_access_api_status,
    const net::IsolationInfo& isolation_info,
    std::vector<mojom::HttpHeaderPtr> additional_headers) {
  DVLOG(3) << "WebSocket::AddChannel @" << reinterpret_cast<void*>(this)
           << " socket_url=\"" << socket_url << "\" requested_protocols=\""
           << base::JoinString(requested_protocols, ", ") << "\" origin=\""
           << origin_ << "\" site_for_cookies=\""
           << site_for_cookies.ToDebugString()
           << "\" storage_access_api_status="
           << static_cast<int>(storage_access_api_status);

  DCHECK(!channel_);

  std::unique_ptr<net::WebSocketEventInterface> event_interface(
      new WebSocketEventHandler(this));
  channel_ = std::make_unique<net::WebSocketChannel>(
      std::move(event_interface), factory_->GetURLRequestContext());

  net::HttpRequestHeaders headers_to_pass;
  for (const auto& header : additional_headers) {
    if (net::HttpUtil::IsValidHeaderName(header->name) &&
        net::HttpUtil::IsValidHeaderValue(header->value) &&
        (net::HttpUtil::IsSafeHeader(header->name, header->value) ||
         base::EqualsCaseInsensitiveASCII(
             header->name, net::HttpRequestHeaders::kUserAgent) ||
         base::EqualsCaseInsensitiveASCII(header->name,
                                          net::HttpRequestHeaders::kCookie) ||
         base::EqualsCaseInsensitiveASCII(header->name, "cookie2"))) {
      headers_to_pass.SetHeader(header->name, header->value);
    }
  }
  channel_->SendAddChannelRequest(socket_url, requested_protocols, origin_,
                                  site_for_cookies, storage_access_api_status,
                                  isolation_info, headers_to_pass,
                                  traffic_annotation_);
}

void WebSocket::OnWritable(MojoResult result,
                           const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    // MOJO_RESULT_FAILED_PRECONDITION (=9) is common when the other end of the
    // pipe is closed.
    DVLOG(1) << "WebSocket::OnWritable mojo error=" << result;

    OnConnectionError(FROM_HERE);
    return;
  }
  SendPendingDataFrames(InterruptionReason::kMojoPipe);
}

void WebSocket::SendPendingDataFrames(InterruptionReason resume_reason) {
  DVLOG(3) << "WebSocket::SendPendingDataFrames @"
           << reinterpret_cast<void*>(this)
           << ", pending_data_frames_.size=" << pending_data_frames_.size()
           << ", incoming_frames_interrupted_="
           << static_cast<int>(incoming_frames_interrupted_);

  if (incoming_frames_interrupted_ != resume_reason)
    return;

  bool resuming_after_interruption = false;
  if (incoming_frames_interrupted_ != InterruptionReason::kNone) {
    incoming_frames_interrupted_ = InterruptionReason::kNone;
    resuming_after_interruption = true;
  }
  while (!pending_data_frames_.empty()) {
    base::span<const char>& data_frame = pending_data_frames_.front();
    if (frame_interceptor_ && resume_reason == InterruptionReason::kNone) {
      // `Intercept` always intercepts sending data per frame.
      auto intercept_result = frame_interceptor_->Intercept(
          WebSocketInterceptor::kIncoming, data_frame.size(),
          base::BindOnce(&WebSocket::SendPendingDataFrames,
                         base::Unretained(this),
                         InterruptionReason::kInterceptor));
      if (intercept_result == WebSocketInterceptor::kShouldWait) {
        DCHECK_EQ(incoming_frames_interrupted_, InterruptionReason::kNone);
        incoming_frames_interrupted_ = InterruptionReason::kInterceptor;
        return;
      }
    }
    SendDataFrame(&data_frame);
    if (data_frame.size() > 0) {
      // Mojo doesn't have any write buffer so far.
      writable_watcher_.ArmOrNotify();
      DCHECK_EQ(incoming_frames_interrupted_, InterruptionReason::kNone);
      incoming_frames_interrupted_ = InterruptionReason::kMojoPipe;
      return;
    }
    pending_data_frames_.pop();
  }
  if (resuming_after_interruption) {
    std::ignore = channel_->ReadFrames();
  }
}

void WebSocket::SendDataFrame(base::span<const char>* payload) {
  DCHECK_GT(payload->size(), 0u);
  MojoResult begin_result;
  base::span<uint8_t> buffer;
  while (payload->size() > 0 && (begin_result = writable_->BeginWriteData(
                                     payload->size(), MOJO_WRITE_DATA_FLAG_NONE,
                                     buffer)) == MOJO_RESULT_OK) {
    const size_t size_to_write = std::min(buffer.size(), payload->size());
    DCHECK_GT(size_to_write, 0u);

    base::as_writable_chars(buffer).copy_prefix_from(
        payload->first(size_to_write));
    *payload = payload->subspan(size_to_write);

    const MojoResult end_result = writable_->EndWriteData(size_to_write);
    DCHECK_EQ(end_result, MOJO_RESULT_OK);
  }
  if (begin_result != MOJO_RESULT_OK &&
      begin_result != MOJO_RESULT_SHOULD_WAIT) {
    DVLOG(1) << "WebSocket::OnWritable mojo error=" << begin_result;
    DCHECK_EQ(begin_result, MOJO_RESULT_FAILED_PRECONDITION);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&WebSocket::OnConnectionError,
                                  weak_ptr_factory_.GetWeakPtr(), FROM_HERE));
  }
  return;
}

void WebSocket::OnReadable(MojoResult result,
                           const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    // MOJO_RESULT_FAILED_PRECONDITION (=9) is common when the other end of the
    // pipe is closed.
    DVLOG(1) << "WebSocket::OnReadable mojo error=" << result;

    OnConnectionError(FROM_HERE);
    return;
  }
  // Safe if ReadAndSendFromDataPipe() deletes |this| because this method is
  // only called from mojo.
  ReadAndSendFromDataPipe(InterruptionReason::kMojoPipe);
}

void WebSocket::ReadAndSendFromDataPipe(InterruptionReason resume_reason) {
  if (outgoing_frames_interrupted_ != resume_reason &&
      outgoing_frames_interrupted_ != InterruptionReason::kNone)
    return;

  if (outgoing_frames_interrupted_ != InterruptionReason::kNone)
    outgoing_frames_interrupted_ = InterruptionReason::kNone;

  while (!pending_send_data_frames_.empty()) {
    DataFrame& data_frame = pending_send_data_frames_.front();
    DVLOG(2) << " ConsumePendingDataFrame frame=(" << data_frame.type
             << ", (data_length = " << data_frame.data_length << "))";
    if (frame_interceptor_ && resume_reason == InterruptionReason::kNone) {
      // `Intercept` always intercepts reading data per frame.
      auto intercept_result = frame_interceptor_->Intercept(
          WebSocketInterceptor::kOutgoing, data_frame.data_length,
          base::BindOnce(&WebSocket::ReadAndSendFromDataPipe,
                         base::Unretained(this),
                         InterruptionReason::kInterceptor));
      if (intercept_result == WebSocketInterceptor::kShouldWait) {
        CHECK_EQ(outgoing_frames_interrupted_, InterruptionReason::kNone);
        outgoing_frames_interrupted_ = InterruptionReason::kInterceptor;
        return;
      }
    }
    if (!ReadAndSendFrameFromDataPipe(&data_frame)) {
      return;
    }
    pending_send_data_frames_.pop();
  }
  if (pending_start_closing_handshake_) {
    std::unique_ptr<CloseInfo> close_info =
        std::move(pending_start_closing_handshake_);
    std::ignore =
        channel_->StartClosingHandshake(close_info->code, close_info->reason);
  }
}

bool WebSocket::ReadAndSendFrameFromDataPipe(DataFrame* data_frame) {
  while (true) {
    if (data_frame->data_length == 0) {
      auto data_to_pass = base::MakeRefCounted<net::IOBufferWithSize>();
      if (channel_->SendFrame(true, MessageTypeToOpCode(data_frame->type),
                              std::move(data_to_pass),
                              0) == net::WebSocketChannel::CHANNEL_DELETED) {
        // |this| has been deleted.
        return false;
      }
      return true;
    }

    base::span<const uint8_t> buffer;
    const MojoResult begin_result =
        readable_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (begin_result == MOJO_RESULT_SHOULD_WAIT) {
      CHECK_EQ(outgoing_frames_interrupted_, InterruptionReason::kNone);
      outgoing_frames_interrupted_ = InterruptionReason::kMojoPipe;
      if (!blocked_on_websocket_channel_) {
        readable_watcher_.ArmOrNotify();
      }
      return false;
    }
    if (begin_result == MOJO_RESULT_FAILED_PRECONDITION) {
      return false;
    }
    CHECK_EQ(begin_result, MOJO_RESULT_OK);

    if (buffer.size() < data_frame->data_length &&
        data_frame->do_not_fragment && !message_under_reassembly_) {
      // The cast is needed to unambiguously select a constructor on 32-bit
      // platforms.
      message_under_reassembly_ = base::MakeRefCounted<net::IOBufferWithSize>(
          base::checked_cast<size_t>(data_frame->data_length));
      CHECK_EQ(bytes_reassembled_, 0u);
    }

    if (message_under_reassembly_) {
      CHECK_GT(data_frame->data_length, bytes_reassembled_);
      const size_t bytes_to_copy = std::min(
          buffer.size(), base::checked_cast<size_t>(data_frame->data_length -
                                                    bytes_reassembled_));
      message_under_reassembly_->span()
          .subspan(bytes_reassembled_)
          .copy_prefix_from(buffer.first(bytes_to_copy));
      bytes_reassembled_ += bytes_to_copy;

      const MojoResult end_result = readable_->EndReadData(bytes_to_copy);
      CHECK_EQ(end_result, MOJO_RESULT_OK);

      CHECK_LE(bytes_reassembled_, data_frame->data_length);
      if (bytes_reassembled_ == data_frame->data_length) {
        bytes_reassembled_ = 0;
        blocked_on_websocket_channel_ = true;
        if (channel_->SendFrame(
                /*fin=*/true, MessageTypeToOpCode(data_frame->type),
                std::move(message_under_reassembly_),
                data_frame->data_length) ==
            net::WebSocketChannel::CHANNEL_DELETED) {
          // |this| has been deleted.
          return false;
        }
        return true;
      }

      continue;
    }

    const size_t size_to_send = std::min(
        buffer.size(), base::saturated_cast<size_t>(data_frame->data_length));
    auto data_to_pass =
        base::MakeRefCounted<net::IOBufferWithSize>(size_to_send);
    data_to_pass->span().copy_prefix_from(buffer.first(size_to_send));

    const bool is_final = (size_to_send == data_frame->data_length);
    blocked_on_websocket_channel_ = true;
    if (channel_->SendFrame(is_final, MessageTypeToOpCode(data_frame->type),
                            std::move(data_to_pass), size_to_send) ==
        net::WebSocketChannel::CHANNEL_DELETED) {
      // |this| has been deleted.
      return false;
    }
    const MojoResult end_result = readable_->EndReadData(size_to_send);
    CHECK_EQ(end_result, MOJO_RESULT_OK);

    if (size_to_send == data_frame->data_length) {
      return true;
    }

    CHECK_GT(data_frame->data_length, size_to_send);
    data_frame->type = mojom::WebSocketMessageType::CONTINUATION;
    data_frame->data_length -= size_to_send;
  }
}

void WebSocket::ResumeDataPipeReading() {
  blocked_on_websocket_channel_ = false;
  readable_watcher_.ArmOrNotify();
}

void WebSocket::OnSSLCertificateErrorResponse(
    std::unique_ptr<net::WebSocketEventInterface::SSLErrorCallbacks> callbacks,
    const net::SSLInfo& ssl_info,
    int net_error) {
  if (net_error == net::OK) {
    callbacks->ContinueSSLRequest();
    return;
  }

  callbacks->CancelSSLRequest(net_error, &ssl_info);
}

void WebSocket::OnAuthRequiredComplete(
    base::OnceCallback<void(const net::AuthCredentials*)> callback,
    const std::optional<net::AuthCredentials>& credentials) {
  DCHECK(!handshake_succeeded_);
  if (!channel_) {
    // Something happened before the authentication response arrives.
    return;
  }

  std::move(callback).Run(credentials ? &*credentials : nullptr);
}

void WebSocket::OnBeforeSendHeadersComplete(
    net::NetworkDelegate::OnBeforeStartTransactionCallback callback,
    int result,
    const std::optional<net::HttpRequestHeaders>& headers) {
  if (!channel_) {
    // Something happened before the OnBeforeSendHeaders response arrives.
    return;
  }
  std::move(callback).Run(result, headers);
}

void WebSocket::OnHeadersReceivedComplete(
    net::CompletionOnceCallback callback,
    scoped_refptr<net::HttpResponseHeaders>* out_headers,
    std::optional<GURL>* out_preserve_fragment_on_redirect_url,
    int result,
    const std::optional<std::string>& headers,
    const std::optional<GURL>& preserve_fragment_on_redirect_url) {
  if (!channel_) {
    // Something happened before the OnHeadersReceived response arrives.
    return;
  }
  if (headers) {
    *out_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(headers.value());
  }
  std::move(callback).Run(result);
}

void WebSocket::Reset() {
  handshake_client_.reset();
  client_.reset();
  auth_handler_.reset();
  header_client_.reset();
  receiver_.reset();

  // net::WebSocketChannel requires that we delete it at this point.
  channel_.reset();

  // deletes |this|.
  factory_->Remove(this);
}

}  // namespace network
