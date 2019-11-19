// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket.h"

#include <inttypes.h>
#include <string.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"  // for WebSocketFrameHeader::OpCode
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "services/network/websocket_factory.h"

namespace network {
namespace {

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

}  // namespace

// Implementation of net::WebSocketEventInterface. Receives events from our
// WebSocketChannel object.
class WebSocket::WebSocketEventHandler final
    : public net::WebSocketEventInterface {
 public:
  explicit WebSocketEventHandler(WebSocket* impl);
  ~WebSocketEventHandler() override;

  // net::WebSocketEventInterface implementation

  void OnCreateURLRequest(net::URLRequest* url_request) override;
  // TODO(yoichio): Merge OnAddChannelResponse and OnFinishOpeningHandshake.
  void OnAddChannelResponse(const std::string& selected_subprotocol,
                            const std::string& extensions,
                            int64_t send_flow_control_quota) override;
  void OnDataFrame(bool fin,
                   WebSocketMessageType type,
                   base::span<const char> payload) override;
  bool HasPendingDataFrames() override;
  void OnClosingHandshake() override;
  void OnSendFlowControlQuotaAdded(int64_t quota) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;
  void OnFailChannel(const std::string& message) override;
  void OnStartOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) override;
  void OnFinishOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) override;
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
      base::Optional<net::AuthCredentials>* credentials) override;

 private:
  WebSocket* const impl_;

  mojom::WebSocketHandshakeResponsePtr response_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebSocketEventHandler);
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
}

void WebSocket::WebSocketEventHandler::OnAddChannelResponse(
    const std::string& selected_protocol,
    const std::string& extensions,
    int64_t send_flow_control_quota) {
  DVLOG(3) << "WebSocketEventHandler::OnAddChannelResponse @"
           << reinterpret_cast<void*>(this) << " selected_protocol=\""
           << selected_protocol << "\""
           << " extensions=\"" << extensions << "\"";

  impl_->handshake_succeeded_ = true;
  impl_->pending_connection_tracker_.OnCompleteHandshake();

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);
  uint64_t receive_quota_threshold =
      net::WebSocketChannel::kReceiveQuotaThreshold;
  if (command_line->HasSwitch(net::kWebSocketReceiveQuotaThreshold)) {
    std::string flag_string =
        command_line->GetSwitchValueASCII(net::kWebSocketReceiveQuotaThreshold);
    if (!base::StringToUint64(flag_string, &receive_quota_threshold))
      receive_quota_threshold = net::WebSocketChannel::kReceiveQuotaThreshold;
  }
  DVLOG(3) << "receive_quota_threshold is " << receive_quota_threshold;

  const MojoCreateDataPipeOptions data_pipe_options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
      receive_quota_threshold * 2};
  mojo::ScopedDataPipeConsumerHandle readable;
  const MojoResult result =
      mojo::CreateDataPipe(&data_pipe_options, &impl_->writable_, &readable);
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

  response_->selected_protocol = selected_protocol;
  response_->extensions = extensions;
  impl_->handshake_client_->OnConnectionEstablished(
      impl_->receiver_.BindNewPipeAndPassRemote(),
      impl_->client_.BindNewPipeAndPassReceiver(), std::move(response_),
      std::move(readable));
  impl_->receiver_.set_disconnect_handler(base::BindOnce(
      &WebSocket::OnConnectionError, base::Unretained(impl_), FROM_HERE));
  impl_->handshake_client_.reset();
  impl_->auth_handler_.reset();
  impl_->header_client_.reset();
  impl_->client_.set_disconnect_handler(base::BindOnce(
      &WebSocket::OnConnectionError, base::Unretained(impl_), FROM_HERE));

  impl_->client_->AddSendFlowControlQuota(send_flow_control_quota);
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
  impl_->SendPendingDataFrames();
}

bool WebSocket::WebSocketEventHandler::HasPendingDataFrames() {
  return !impl_->pending_data_frames_.empty();
}

void WebSocket::WebSocketEventHandler::OnClosingHandshake() {
  DVLOG(3) << "WebSocketEventHandler::OnClosingHandshake @"
           << reinterpret_cast<void*>(this);

  impl_->client_->OnClosingHandshake();
}

void WebSocket::WebSocketEventHandler::OnSendFlowControlQuotaAdded(
    int64_t quota) {
  DVLOG(3) << "WebSocketEventHandler::OnSendFlowControlQuotaAdded @"
           << reinterpret_cast<void*>(this) << " quota=" << quota;

  impl_->client_->AddSendFlowControlQuota(quota);
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
    const std::string& message) {
  DVLOG(3) << "WebSocketEventHandler::OnFailChannel @"
           << reinterpret_cast<void*>(this) << " message=\"" << message << "\"";

  impl_->handshake_client_.ResetWithReason(mojom::WebSocket::kInternalFailure,
                                           message);
  impl_->client_.ResetWithReason(mojom::WebSocket::kInternalFailure, message);
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

void WebSocket::WebSocketEventHandler::OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  DVLOG(3) << "WebSocketEventHandler::OnFinishOpeningHandshake "
           << reinterpret_cast<void*>(this)
           << " CanReadRawCookies=" << impl_->has_raw_headers_access_;

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
    if (impl_->has_raw_headers_access_ ||
        !net::HttpResponseHeaders::IsCookieResponseHeader(name)) {
      // We drop cookie-related headers such as "set-cookie" when the
      // renderer doesn't have access.
      response_to_pass->headers.push_back(mojom::HttpHeader::New(name, value));
      base::StrAppend(&headers_text, {name, ": ", value, "\r\n"});
    }
  }
  headers_text.append("\r\n");
  response_to_pass->headers_text = headers_text;

  response_ = std::move(response_to_pass);
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
  impl_->factory_->OnSSLCertificateError(
      base::BindOnce(&WebSocket::OnSSLCertificateErrorResponse,
                     impl_->weak_ptr_factory_.GetWeakPtr(),
                     std::move(callbacks), ssl_info),
      url, impl_->child_id_, impl_->frame_id_, net_error, ssl_info, fatal);
}

int WebSocket::WebSocketEventHandler::OnAuthRequired(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const net::IPEndPoint& remote_endpoint,
    base::OnceCallback<void(const net::AuthCredentials*)> callback,
    base::Optional<net::AuthCredentials>* credentials) {
  DVLOG(3) << "WebSocketEventHandler::OnAuthRequired"
           << reinterpret_cast<void*>(this);
  if (!impl_->auth_handler_) {
    *credentials = base::nullopt;
    return net::OK;
  }

  impl_->auth_handler_->OnAuthRequired(
      auth_info, std::move(response_headers), remote_endpoint,
      base::BindOnce(&WebSocket::OnAuthRequiredComplete,
                     impl_->weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
  return net::ERR_IO_PENDING;
}

WebSocket::WebSocket(
    WebSocketFactory* factory,
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    const net::NetworkIsolationKey& network_isolation_key,
    std::vector<mojom::HttpHeaderPtr> additional_headers,
    int32_t child_id,
    int32_t frame_id,
    const url::Origin& origin,
    uint32_t options,
    HasRawHeadersAccess has_raw_headers_access,
    mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
    mojo::PendingRemote<mojom::AuthenticationHandler> auth_handler,
    mojo::PendingRemote<mojom::TrustedHeaderClient> header_client,
    WebSocketThrottler::PendingConnection pending_connection_tracker,
    base::TimeDelta delay)
    : factory_(factory),
      handshake_client_(std::move(handshake_client)),
      auth_handler_(std::move(auth_handler)),
      header_client_(std::move(header_client)),
      pending_connection_tracker_(std::move(pending_connection_tracker)),
      delay_(delay),
      options_(options),
      child_id_(child_id),
      frame_id_(frame_id),
      origin_(std::move(origin)),
      has_raw_headers_access_(has_raw_headers_access),
      writable_watcher_(FROM_HERE,
                        mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                        base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(handshake_client_);
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
  if (delay_ > base::TimeDelta()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebSocket::AddChannel, weak_ptr_factory_.GetWeakPtr(),
                       url, requested_protocols, site_for_cookies,
                       network_isolation_key, std::move(additional_headers)),
        delay_);
    return;
  }
  AddChannel(url, requested_protocols, site_for_cookies, network_isolation_key,
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

void WebSocket::SendFrame(bool fin,
                          mojom::WebSocketMessageType type,
                          base::span<const uint8_t> data) {
  DVLOG(3) << "WebSocket::SendFrame @" << reinterpret_cast<void*>(this)
           << " fin=" << fin << " type=" << type << " data is " << data.size()
           << " bytes";

  DCHECK(channel_)
      << "WebSocket::SendFrame is called but there is no active channel.";
  DCHECK(handshake_succeeded_);
  // This is guaranteed by the maximum size enforced on mojo messages.
  DCHECK_LE(data.size(), static_cast<size_t>(INT_MAX));

  // This is guaranteed by mojo.
  DCHECK(IsKnownEnumValue(type));

  // TODO(darin): Avoid this copy.
  auto data_to_pass = base::MakeRefCounted<net::IOBuffer>(data.size());
  memcpy(data_to_pass->data(), data.data(), data.size());

  channel_->SendFrame(fin, MessageTypeToOpCode(type), std::move(data_to_pass),
                      data.size());
}

void WebSocket::StartReceiving() {
  DCHECK(pending_data_frames_.empty());
  ignore_result(channel_->ReadFrames());
}

void WebSocket::StartClosingHandshake(uint16_t code,
                                      const std::string& reason) {
  DVLOG(3) << "WebSocket::StartClosingHandshake @"
           << reinterpret_cast<void*>(this) << " code=" << code << " reason=\""
           << reason << "\"";

  DCHECK(channel_)
      << "WebSocket::SendFrame is called but there is no active channel.";
  DCHECK(handshake_succeeded_);
  ignore_result(channel_->StartClosingHandshake(code, reason));
}

bool WebSocket::AllowCookies(const GURL& url) const {
  const GURL site_for_cookies = origin_.GetURL();
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
             url, site_for_cookies) == net::OK;
}

int WebSocket::OnBeforeStartTransaction(net::CompletionOnceCallback callback,
                                        net::HttpRequestHeaders* headers) {
  if (header_client_) {
    header_client_->OnBeforeSendHeaders(
        *headers, base::BindOnce(&WebSocket::OnBeforeSendHeadersComplete,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback), headers));
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

int WebSocket::OnHeadersReceived(
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    base::Optional<GURL>* preserve_fragment_on_redirect_url) {
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
    const GURL& site_for_cookies,
    const net::NetworkIsolationKey& network_isolation_key,
    std::vector<mojom::HttpHeaderPtr> additional_headers) {
  DVLOG(3) << "WebSocket::AddChannel @" << reinterpret_cast<void*>(this)
           << " socket_url=\"" << socket_url << "\" requested_protocols=\""
           << base::JoinString(requested_protocols, ", ") << "\" origin=\""
           << origin_ << "\" site_for_cookies=\"" << site_for_cookies << "\"";

  DCHECK(!channel_);

  std::unique_ptr<net::WebSocketEventInterface> event_interface(
      new WebSocketEventHandler(this));
  channel_.reset(new net::WebSocketChannel(std::move(event_interface),
                                           factory_->GetURLRequestContext()));

  net::HttpRequestHeaders headers_to_pass;
  for (const auto& header : additional_headers) {
    if (net::HttpUtil::IsValidHeaderName(header->name) &&
        net::HttpUtil::IsValidHeaderValue(header->value) &&
        (net::HttpUtil::IsSafeHeader(header->name) ||
         base::EqualsCaseInsensitiveASCII(
             header->name, net::HttpRequestHeaders::kUserAgent) ||
         base::EqualsCaseInsensitiveASCII(header->name,
                                          net::HttpRequestHeaders::kCookie) ||
         base::EqualsCaseInsensitiveASCII(header->name, "cookie2"))) {
      headers_to_pass.SetHeader(header->name, header->value);
    }
  }
  channel_->SendAddChannelRequest(socket_url, requested_protocols, origin_,
                                  site_for_cookies, network_isolation_key,
                                  headers_to_pass);
}

void WebSocket::OnWritable(MojoResult result,
                           const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << "WebSocket::OnWritable mojo error=" << result;
    Reset();
    return;
  }
  wait_for_writable_ = false;
  SendPendingDataFrames();
  if (pending_data_frames_.empty()) {
    ignore_result(channel_->ReadFrames());
  }
}

void WebSocket::SendPendingDataFrames() {
  DVLOG(3) << "WebSocket::SendPendingDataFrames @"
           << reinterpret_cast<void*>(this)
           << ", pending_data_frames_.size=" << pending_data_frames_.size()
           << ", wait_for_writable_?" << wait_for_writable_;
  if (wait_for_writable_) {
    return;
  }
  while (!pending_data_frames_.empty()) {
    base::span<const char>& data_frame = pending_data_frames_.front();
    SendDataFrame(&data_frame);
    if (data_frame.size() > 0) {
      // Mojo doesn't have any write buffer so far.
      writable_watcher_.ArmOrNotify();
      wait_for_writable_ = true;
      return;
    }
    pending_data_frames_.pop();
  }
}

void WebSocket::SendDataFrame(base::span<const char>* payload) {
  DCHECK_GT(payload->size(), 0u);
  MojoResult begin_result;
  void* buffer;
  uint32_t writable_size = 0;
  while (payload->size() > 0 &&
         (begin_result = writable_->BeginWriteData(
              &buffer, &writable_size, MOJO_WRITE_DATA_FLAG_NONE)) ==
             MOJO_RESULT_OK) {
    const uint32_t size_to_write =
        std::min(writable_size, static_cast<uint32_t>(payload->size()));
    DCHECK_GT(size_to_write, 0u);

    memcpy(buffer, payload->data(), size_to_write);
    *payload = payload->subspan(size_to_write);

    const MojoResult end_result = writable_->EndWriteData(size_to_write);
    DCHECK_EQ(end_result, MOJO_RESULT_OK);
  }
  if (begin_result != MOJO_RESULT_OK &&
      begin_result != MOJO_RESULT_SHOULD_WAIT) {
    DVLOG(1) << "WebSocket::OnWritable mojo error=" << begin_result;
    DCHECK_EQ(begin_result, MOJO_RESULT_FAILED_PRECONDITION);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WebSocket::OnConnectionError,
                                  weak_ptr_factory_.GetWeakPtr(), FROM_HERE));
  }
  return;
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
    const base::Optional<net::AuthCredentials>& credentials) {
  DCHECK(!handshake_succeeded_);
  if (!channel_) {
    // Something happened before the authentication response arrives.
    return;
  }

  std::move(callback).Run(credentials ? &*credentials : nullptr);
}

void WebSocket::OnBeforeSendHeadersComplete(
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* out_headers,
    int result,
    const base::Optional<net::HttpRequestHeaders>& headers) {
  if (!channel_) {
    // Something happened before the OnBeforeSendHeaders response arrives.
    return;
  }
  if (headers)
    *out_headers = headers.value();
  std::move(callback).Run(result);
}

void WebSocket::OnHeadersReceivedComplete(
    net::CompletionOnceCallback callback,
    scoped_refptr<net::HttpResponseHeaders>* out_headers,
    base::Optional<GURL>* out_preserve_fragment_on_redirect_url,
    int result,
    const base::Optional<std::string>& headers,
    const base::Optional<GURL>& preserve_fragment_on_redirect_url) {
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
