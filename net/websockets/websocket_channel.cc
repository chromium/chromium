// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/websockets/websocket_channel.h"

#include <limits.h>  // for INT_MAX
#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "net/websockets/websocket_stream.h"

namespace net {
class AuthChallengeInfo;
class AuthCredentials;
class SSLInfo;

namespace {

using base::StreamingUtf8Validator;

constexpr size_t kWebSocketCloseCodeLength = 2;
// Timeout for waiting for the server to acknowledge a closing handshake.
constexpr int kClosingHandshakeTimeoutSeconds = 60;
// We wait for the server to close the underlying connection as recommended in
// https://tools.ietf.org/html/rfc6455#section-7.1.1
// We don't use 2MSL since there're server implementations that don't follow
// the recommendation and wait for the client to close the underlying
// connection. It leads to unnecessarily long time before CloseEvent
// invocation. We want to avoid this rather than strictly following the spec
// recommendation.
constexpr int kUnderlyingConnectionCloseTimeoutSeconds = 2;

using ChannelState = WebSocketChannel::ChannelState;

// Maximum close reason length = max control frame payload -
//                               status code length
//                             = 125 - 2
constexpr size_t kMaximumCloseReasonLength = 125 - kWebSocketCloseCodeLength;

// Check a close status code for strict compliance with RFC6455. This is only
// used for close codes received from a renderer that we are intending to send
// out over the network. See ParseClose() for the restrictions on incoming close
// codes. The |code| parameter is type int for convenience of implementation;
// the real type is uint16_t. Code 1005 is treated specially; it cannot be set
// explicitly by Javascript but the renderer uses it to indicate we should send
// a Close frame with no payload.
bool IsStrictlyValidCloseStatusCode(int code) {
  static constexpr int kInvalidRanges[] = {
      // [BAD, OK)
      0,    1000,   // 1000 is the first valid code
      1006, 1007,   // 1006 MUST NOT be set.
      1014, 3000,   // 1014 unassigned; 1015 up to 2999 are reserved.
      5000, 65536,  // Codes above 5000 are invalid.
  };
  const int* const kInvalidRangesEnd =
      kInvalidRanges + std::size(kInvalidRanges);

  DCHECK_GE(code, 0);
  DCHECK_LT(code, 65536);
  const int* upper = std::upper_bound(kInvalidRanges, kInvalidRangesEnd, code);
  DCHECK_NE(kInvalidRangesEnd, upper);
  DCHECK_GT(upper, kInvalidRanges);
  DCHECK_GT(*upper, code);
  DCHECK_LE(*(upper - 1), code);
  return ((upper - kInvalidRanges) % 2) == 0;
}

// Sets |name| to the name of the frame type for the given |opcode|. Note that
// for all of Text, Binary and Continuation opcode, this method returns
// "Data frame".
void GetFrameTypeForOpcode(WebSocketFrameHeader::OpCode opcode,
                           std::string* name) {
  switch (opcode) {
    case WebSocketFrameHeader::kOpCodeText:    // fall-thru
    case WebSocketFrameHeader::kOpCodeBinary:  // fall-thru
    case WebSocketFrameHeader::kOpCodeContinuation:
      *name = "Data frame";
      break;

    case WebSocketFrameHeader::kOpCodePing:
      *name = "Ping";
      break;

    case WebSocketFrameHeader::kOpCodePong:
      *name = "Pong";
      break;

    case WebSocketFrameHeader::kOpCodeClose:
      *name = "Close";
      break;

    default:
      *name = "Unknown frame type";
      break;
  }

  return;
}

base::Value::Dict NetLogFailParam(uint16_t code,
                                  std::string_view reason,
                                  std::string_view message) {
  base::Value::Dict dict;
  dict.Set("code", code);
  dict.Set("reason", reason);
  dict.Set("internal_reason", message);
  return dict;
}

class DependentIOBuffer : public WrappedIOBuffer {
 public:
  DependentIOBuffer(scoped_refptr<IOBufferWithSize> buffer, size_t offset)
      : WrappedIOBuffer(buffer->span().subspan(offset)),
        buffer_(std::move(buffer)) {}

 private:
  ~DependentIOBuffer() override {
    // Prevent `data_` from dangling should this destructor remove the
    // last reference to `buffer_`.
    data_ = nullptr;
  }

  scoped_refptr<IOBufferWithSize> buffer_;
};

}  // namespace

// A class to encapsulate a set of frames and information about the size of
// those frames.
class WebSocketChannel::SendBuffer {
 public:
  SendBuffer() = default;

  // Add a WebSocketFrame to the buffer and increase total_bytes_.
  void AddFrame(std::unique_ptr<WebSocketFrame> chunk,
                scoped_refptr<IOBuffer> buffer);

  // Return a pointer to the frames_ for write purposes.
  std::vector<std::unique_ptr<WebSocketFrame>>* frames() { return &frames_; }

 private:
  // The frames_ that will be sent in the next call to WriteFrames().
  std::vector<std::unique_ptr<WebSocketFrame>> frames_;
  // References of each WebSocketFrame.data;
  std::vector<scoped_refptr<IOBuffer>> buffers_;

  // The total size of the payload data in |frames_|. This will be used to
  // measure the throughput of the link.
  // TODO(ricea): Measure the throughput of the link.
  uint64_t total_bytes_ = 0;
};

void WebSocketChannel::SendBuffer::AddFrame(
    std::unique_ptr<WebSocketFrame> frame,
    scoped_refptr<IOBuffer> buffer) {
  total_bytes_ += frame->header.payload_length;
  frames_.push_back(std::move(frame));
  buffers_.push_back(std::move(buffer));
}

// Implementation of WebSocketStream::ConnectDelegate that simply forwards the
// calls on to the WebSocketChannel that created it.
class WebSocketChannel::ConnectDelegate
    : public WebSocketStream::ConnectDelegate {
 public:
  explicit ConnectDelegate(WebSocketChannel* creator) : creator_(creator) {}

  ConnectDelegate(const ConnectDelegate&) = delete;
  ConnectDelegate& operator=(const ConnectDelegate&) = delete;

  void OnCreateRequest(URLRequest* request) override {
    creator_->OnCreateURLRequest(request);
  }

  void OnURLRequestConnected(URLRequest* request,
                             const TransportInfo& info) override {
    creator_->OnURLRequestConnected(request, info);
  }

  void OnSuccess(
      std::unique_ptr<WebSocketStream> stream,
      std::unique_ptr<WebSocketHandshakeResponseInfo> response) override {
    creator_->OnConnectSuccess(std::move(stream), std::move(response));
    // |this| may have been deleted.
  }

  void OnFailure(const std::string& message,
                 int net_error,
                 std::optional<int> response_code) override {
    creator_->OnConnectFailure(message, net_error, response_code);
    // |this| has been deleted.
  }

  void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo> request) override {
    creator_->OnStartOpeningHandshake(std::move(request));
  }

  void OnSSLCertificateError(
      std::unique_ptr<WebSocketEventInterface::SSLErrorCallbacks>
          ssl_error_callbacks,
      int net_error,
      const SSLInfo& ssl_info,
      bool fatal) override {
    creator_->OnSSLCertificateError(std::move(ssl_error_callbacks), net_error,
                                    ssl_info, fatal);
  }

  int OnAuthRequired(const AuthChallengeInfo& auth_info,
                     scoped_refptr<HttpResponseHeaders> headers,
                     const IPEndPoint& remote_endpoint,
                     base::OnceCallback<void(const AuthCredentials*)> callback,
                     std::optional<AuthCredentials>* credentials) override {
    return creator_->OnAuthRequired(auth_info, std::move(headers),
                                    remote_endpoint, std::move(callback),
                                    credentials);
  }

 private:
  // A pointer to the WebSocketChannel that created this object. There is no
  // danger of this pointer being stale, because deleting the WebSocketChannel
  // cancels the connect process, deleting this object and preventing its
  // callbacks from being called.
  const raw_ptr<WebSocketChannel> creator_;
};

WebSocketChannel::WebSocketChannel(
    std::unique_ptr<WebSocketEventInterface> event_interface,
    URLRequestContext* url_request_context)
    : event_interface_(std::move(event_interface)),
      url_request_context_(url_request_context),
      closing_handshake_timeout_(
          base::Seconds(kClosingHandshakeTimeoutSeconds)),
      underlying_connection_close_timeout_(
          base::Seconds(kUnderlyingConnectionCloseTimeoutSeconds)) {}

WebSocketChannel::~WebSocketChannel() {
  // The stream may hold a pointer to read_frames_, and so it needs to be
  // destroyed first.
  stream_.reset();
  // The timer may have a callback pointing back to us, so stop it just in case
  // someone decides to run the event loop from their destructor.
  close_timer_.Stop();
}

void WebSocketChannel::SendAddChannelRequest(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const url::Origin& origin,
    const SiteForCookies& site_for_cookies,
    StorageAccessApiStatus storage_access_api_status,
    const IsolationInfo& isolation_info,
    const HttpRequestHeaders& additional_headers,
    NetworkTrafficAnnotationTag traffic_annotation) {
  SendAddChannelRequestWithSuppliedCallback(
      socket_url, requested_subprotocols, origin, site_for_cookies,
      storage_access_api_status, isolation_info, additional_headers,
      traffic_annotation,
      base::BindOnce(&WebSocketStream::CreateAndConnectStream));
}

void WebSocketChannel::SetState(State new_state) {
  DCHECK_NE(state_, new_state);

  state_ = new_state;
}

bool WebSocketChannel::InClosingState() const {
  // The state RECV_CLOSED is not supported here, because it is only used in one
  // code path and should not leak into the code in general.
  DCHECK_NE(RECV_CLOSED, state_)
      << "InClosingState called with state_ == RECV_CLOSED";
  return state_ == SEND_CLOSED || state_ == CLOSE_WAIT || state_ == CLOSED;
}

WebSocketChannel::ChannelState WebSocketChannel::SendFrame(
    bool fin,
    WebSocketFrameHeader::OpCode op_code,
    scoped_refptr<IOBuffer> buffer,
    size_t buffer_size) {
  DCHECK_LE(buffer_size, static_cast<size_t>(INT_MAX));
  DCHECK(stream_) << "Got SendFrame without a connection established; fin="
                  << fin << " op_code=" << op_code
                  << " buffer_size=" << buffer_size;

  if (InClosingState()) {
    DVLOG(1) << "SendFrame called in state " << state_
             << ". This may be a bug, or a harmless race.";
    return CHANNEL_ALIVE;
  }

  DCHECK_EQ(state_, CONNECTED);

  DCHECK(WebSocketFrameHeader::IsKnownDataOpCode(op_code))
      << "Got SendFrame with bogus op_code " << op_code << " fin=" << fin
      << " buffer_size=" << buffer_size;

  if (op_code == WebSocketFrameHeader::kOpCodeText ||
      (op_code == WebSocketFrameHeader::kOpCodeContinuation &&
       sending_text_message_)) {
    StreamingUtf8Validator::State state = outgoing_utf8_validator_.AddBytes(
        base::make_span(buffer->bytes(), buffer_size));
    if (state == StreamingUtf8Validator::INVALID ||
        (state == StreamingUtf8Validator::VALID_MIDPOINT && fin)) {
      // TODO(ricea): Kill renderer.
      FailChannel("Browser sent a text frame containing invalid UTF-8",
                  kWebSocketErrorGoingAway, "");
      return CHANNEL_DELETED;
      // |this| has been deleted.
    }
    sending_text_message_ = !fin;
    DCHECK(!fin || state == StreamingUtf8Validator::VALID_ENDPOINT);
  }

  return SendFrameInternal(fin, op_code, std::move(buffer), buffer_size);
  // |this| may have been deleted.
}

ChannelState WebSocketChannel::StartClosingHandshake(
    uint16_t code,
    const std::string& reason) {
  if (InClosingState()) {
    // When the associated renderer process is killed while the channel is in
    // CLOSING state we reach here.
    DVLOG(1) << "StartClosingHandshake called in state " << state_
             << ". This may be a bug, or a harmless race.";
    return CHANNEL_ALIVE;
  }
  if (has_received_close_frame_) {
    // We reach here if the client wants to start a closing handshake while
    // the browser is waiting for the client to consume incoming data frames
    // before responding to a closing handshake initiated by the server.
    // As the client doesn't want the data frames any more, we can respond to
    // the closing handshake initiated by the server.
    return RespondToClosingHandshake();
  }
  if (state_ == CONNECTING) {
    // Abort the in-progress handshake and drop the connection immediately.
    stream_request_.reset();
    SetState(CLOSED);
    DoDropChannel(false, kWebSocketErrorAbnormalClosure, "");
    return CHANNEL_DELETED;
  }
  DCHECK_EQ(state_, CONNECTED);

  DCHECK(!close_timer_.IsRunning());
  // This use of base::Unretained() is safe because we stop the timer in the
  // destructor.
  close_timer_.Start(
      FROM_HERE, closing_handshake_timeout_,
      base::BindOnce(&WebSocketChannel::CloseTimeout, base::Unretained(this)));

  // Javascript actually only permits 1000 and 3000-4999, but the implementation
  // itself may produce different codes. The length of |reason| is also checked
  // by Javascript.
  if (!IsStrictlyValidCloseStatusCode(code) ||
      reason.size() > kMaximumCloseReasonLength) {
    // "InternalServerError" is actually used for errors from any endpoint, per
    // errata 3227 to RFC6455. If the renderer is sending us an invalid code or
    // reason it must be malfunctioning in some way, and based on that we
    // interpret this as an internal error.
    if (SendClose(kWebSocketErrorInternalServerError, "") == CHANNEL_DELETED)
      return CHANNEL_DELETED;
    DCHECK_EQ(CONNECTED, state_);
    SetState(SEND_CLOSED);
    return CHANNEL_ALIVE;
  }
  if (SendClose(code, StreamingUtf8Validator::Validate(reason)
                          ? reason
                          : std::string()) == CHANNEL_DELETED)
    return CHANNEL_DELETED;
  DCHECK_EQ(CONNECTED, state_);
  SetState(SEND_CLOSED);
  return CHANNEL_ALIVE;
}

void WebSocketChannel::SendAddChannelRequestForTesting(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const url::Origin& origin,
    const SiteForCookies& site_for_cookies,
    StorageAccessApiStatus storage_access_api_status,
    const IsolationInfo& isolation_info,
    const HttpRequestHeaders& additional_headers,
    NetworkTrafficAnnotationTag traffic_annotation,
    WebSocketStreamRequestCreationCallback callback) {
  SendAddChannelRequestWithSuppliedCallback(
      socket_url, requested_subprotocols, origin, site_for_cookies,
      storage_access_api_status, isolation_info, additional_headers,
      traffic_annotation, std::move(callback));
}

void WebSocketChannel::SetClosingHandshakeTimeoutForTesting(
    base::TimeDelta delay) {
  closing_handshake_timeout_ = delay;
}

void WebSocketChannel::SetUnderlyingConnectionCloseTimeoutForTesting(
    base::TimeDelta delay) {
  underlying_connection_close_timeout_ = delay;
}

void WebSocketChannel::SendAddChannelRequestWithSuppliedCallback(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const url::Origin& origin,
    const SiteForCookies& site_for_cookies,
    StorageAccessApiStatus storage_access_api_status,
    const IsolationInfo& isolation_info,
    const HttpRequestHeaders& additional_headers,
    NetworkTrafficAnnotationTag traffic_annotation,
    WebSocketStreamRequestCreationCallback callback) {
  DCHECK_EQ(FRESHLY_CONSTRUCTED, state_);
  if (!socket_url.SchemeIsWSOrWSS()) {
    // TODO(ricea): Kill the renderer (this error should have been caught by
    // Javascript).
    event_interface_->OnFailChannel("Invalid scheme", ERR_FAILED, std::nullopt);
    // |this| is deleted here.
    return;
  }
  socket_url_ = socket_url;
  auto connect_delegate = std::make_unique<ConnectDelegate>(this);
  stream_request_ = std::move(callback).Run(
      socket_url_, requested_subprotocols, origin, site_for_cookies,
      storage_access_api_status, isolation_info, additional_headers,
      url_request_context_.get(), NetLogWithSource(), traffic_annotation,
      std::move(connect_delegate));
  SetState(CONNECTING);
}

void WebSocketChannel::OnCreateURLRequest(URLRequest* request) {
  event_interface_->OnCreateURLRequest(request);
}

void WebSocketChannel::OnURLRequestConnected(URLRequest* request,
                                             const TransportInfo& info) {
  event_interface_->OnURLRequestConnected(request, info);
}

void WebSocketChannel::OnConnectSuccess(
    std::unique_ptr<WebSocketStream> stream,
    std::unique_ptr<WebSocketHandshakeResponseInfo> response) {
  DCHECK(stream);
  DCHECK_EQ(CONNECTING, state_);

  stream_ = std::move(stream);

  SetState(CONNECTED);

  // |stream_request_| is not used once the connection has succeeded.
  stream_request_.reset();

  event_interface_->OnAddChannelResponse(
      std::move(response), stream_->GetSubProtocol(), stream_->GetExtensions());
  // |this| may have been deleted after OnAddChannelResponse.
}

void WebSocketChannel::OnConnectFailure(const std::string& message,
                                        int net_error,
                                        std::optional<int> response_code) {
  DCHECK_EQ(CONNECTING, state_);

  // Copy the message before we delete its owner.
  std::string message_copy = message;

  SetState(CLOSED);
  stream_request_.reset();

  event_interface_->OnFailChannel(message_copy, net_error, response_code);
  // |this| has been deleted.
}

void WebSocketChannel::OnSSLCertificateError(
    std::unique_ptr<WebSocketEventInterface::SSLErrorCallbacks>
        ssl_error_callbacks,
    int net_error,
    const SSLInfo& ssl_info,
    bool fatal) {
  event_interface_->OnSSLCertificateError(
      std::move(ssl_error_callbacks), socket_url_, net_error, ssl_info, fatal);
}

int WebSocketChannel::OnAuthRequired(
    const AuthChallengeInfo& auth_info,
    scoped_refptr<HttpResponseHeaders> response_headers,
    const IPEndPoint& remote_endpoint,
    base::OnceCallback<void(const AuthCredentials*)> callback,
    std::optional<AuthCredentials>* credentials) {
  return event_interface_->OnAuthRequired(
      auth_info, std::move(response_headers), remote_endpoint,
      std::move(callback), credentials);
}

void WebSocketChannel::OnStartOpeningHandshake(
    std::unique_ptr<WebSocketHandshakeRequestInfo> request) {
  event_interface_->OnStartOpeningHandshake(std::move(request));
}

ChannelState WebSocketChannel::WriteFrames() {
  int result = OK;
  do {
    // This use of base::Unretained is safe because this object owns the
    // WebSocketStream and destroying it cancels all callbacks.
    result = stream_->WriteFrames(
        data_being_sent_->frames(),
        base::BindOnce(base::IgnoreResult(&WebSocketChannel::OnWriteDone),
                       base::Unretained(this), false));
    if (result != ERR_IO_PENDING) {
      if (OnWriteDone(true, result) == CHANNEL_DELETED)
        return CHANNEL_DELETED;
      // OnWriteDone() returns CHANNEL_DELETED on error. Here |state_| is
      // guaranteed to be the same as before OnWriteDone() call.
    }
  } while (result == OK && data_being_sent_);
  return CHANNEL_ALIVE;
}

ChannelState WebSocketChannel::OnWriteDone(bool synchronous, int result) {
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(data_being_sent_);
  switch (result) {
    case OK:
      if (data_to_send_next_) {
        data_being_sent_ = std::move(data_to_send_next_);
        if (!synchronous)
          return WriteFrames();
      } else {
        data_being_sent_.reset();
        event_interface_->OnSendDataFrameDone();
      }
      return CHANNEL_ALIVE;

    // If a recoverable error condition existed, it would go here.

    default:
      DCHECK_LT(result, 0)
          << "WriteFrames() should only return OK or ERR_ codes";

      stream_->Close();
      SetState(CLOSED);
      DoDropChannel(false, kWebSocketErrorAbnormalClosure, "");
      return CHANNEL_DELETED;
  }
}

ChannelState WebSocketChannel::ReadFrames() {
  DCHECK(stream_);
  DCHECK(state_ == CONNECTED || state_ == SEND_CLOSED || state_ == CLOSE_WAIT);
  DCHECK(read_frames_.empty());
  if (is_reading_) {
    return CHANNEL_ALIVE;
  }

  if (!InClosingState() && has_received_close_frame_) {
    DCHECK(!event_interface_->HasPendingDataFrames());
    // We've been waiting for the client to consume the frames before
    // responding to the closing handshake initiated by the server.
    if (RespondToClosingHandshake() == CHANNEL_DELETED) {
      return CHANNEL_DELETED;
    }
  }

  // TODO(crbug.com/41479064): Remove this CHECK.
  CHECK(event_interface_);
  while (!event_interface_->HasPendingDataFrames()) {
    DCHECK(stream_);
    // This use of base::Unretained is safe because this object owns the
    // WebSocketStream, and any pending reads will be cancelled when it is
    // destroyed.
    const int result = stream_->ReadFrames(
        &read_frames_,
        base::BindOnce(base::IgnoreResult(&WebSocketChannel::OnReadDone),
                       base::Unretained(this), false));
    if (result == ERR_IO_PENDING) {
      is_reading_ = true;
      return CHANNEL_ALIVE;
    }
    if (OnReadDone(true, result) == CHANNEL_DELETED) {
      return CHANNEL_DELETED;
    }
    DCHECK_NE(CLOSED, state_);
    // TODO(crbug.com/41479064): Remove this CHECK.
    CHECK(event_interface_);
  }
  return CHANNEL_ALIVE;
}

ChannelState WebSocketChannel::OnReadDone(bool synchronous, int result) {
  DVLOG(3) << "WebSocketChannel::OnReadDone synchronous?" << synchronous
           << ", result=" << result
           << ", read_frames_.size=" << read_frames_.size();
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  DCHECK_NE(ERR_IO_PENDING, result);
  switch (result) {
    case OK:
      // ReadFrames() must use ERR_CONNECTION_CLOSED for a closed connection
      // with no data read, not an empty response.
      DCHECK(!read_frames_.empty())
          << "ReadFrames() returned OK, but nothing was read.";
      for (auto& read_frame : read_frames_) {
        if (HandleFrame(std::move(read_frame)) == CHANNEL_DELETED)
          return CHANNEL_DELETED;
      }
      read_frames_.clear();
      DCHECK_NE(CLOSED, state_);
      if (!synchronous) {
        is_reading_ = false;
        if (!event_interface_->HasPendingDataFrames()) {
          return ReadFrames();
        }
      }
      return CHANNEL_ALIVE;

    case ERR_WS_PROTOCOL_ERROR:
      // This could be kWebSocketErrorProtocolError (specifically, non-minimal
      // encoding of payload length) or kWebSocketErrorMessageTooBig, or an
      // extension-specific error.
      FailChannel("Invalid frame header", kWebSocketErrorProtocolError,
                  "WebSocket Protocol Error");
      return CHANNEL_DELETED;

    default:
      DCHECK_LT(result, 0)
          << "ReadFrames() should only return OK or ERR_ codes";

      stream_->Close();
      SetState(CLOSED);

      uint16_t code = kWebSocketErrorAbnormalClosure;
      std::string reason = "";
      bool was_clean = false;
      if (has_received_close_frame_) {
        code = received_close_code_;
        reason = received_close_reason_;
        was_clean = (result == ERR_CONNECTION_CLOSED);
      }

      DoDropChannel(was_clean, code, reason);
      return CHANNEL_DELETED;
  }
}

ChannelState WebSocketChannel::HandleFrame(
    std::unique_ptr<WebSocketFrame> frame) {
  if (frame->header.masked) {
    // RFC6455 Section 5.1 "A client MUST close a connection if it detects a
    // masked frame."
    FailChannel(
        "A server must not mask any frames that it sends to the "
        "client.",
        kWebSocketErrorProtocolError, "Masked frame from server");
    return CHANNEL_DELETED;
  }
  const WebSocketFrameHeader::OpCode opcode = frame->header.opcode;
  DCHECK(!WebSocketFrameHeader::IsKnownControlOpCode(opcode) ||
         frame->header.final);
  if (frame->header.reserved1 || frame->header.reserved2 ||
      frame->header.reserved3) {
    FailChannel(
        base::StringPrintf("One or more reserved bits are on: reserved1 = %d, "
                           "reserved2 = %d, reserved3 = %d",
                           static_cast<int>(frame->header.reserved1),
                           static_cast<int>(frame->header.reserved2),
                           static_cast<int>(frame->header.reserved3)),
        kWebSocketErrorProtocolError, "Invalid reserved bit");
    return CHANNEL_DELETED;
  }

  // Respond to the frame appropriately to its type.
  return HandleFrameByState(
      opcode, frame->header.final,
      base::make_span(frame->payload, base::checked_cast<size_t>(
                                          frame->header.payload_length)));
}

ChannelState WebSocketChannel::HandleFrameByState(
    const WebSocketFrameHeader::OpCode opcode,
    bool final,
    base::span<const char> payload) {
  DCHECK_NE(RECV_CLOSED, state_)
      << "HandleFrame() does not support being called re-entrantly from within "
         "SendClose()";
  DCHECK_NE(CLOSED, state_);
  if (state_ == CLOSE_WAIT) {
    std::string frame_name;
    GetFrameTypeForOpcode(opcode, &frame_name);

    // FailChannel() won't send another Close frame.
    FailChannel(frame_name + " received after close",
                kWebSocketErrorProtocolError, "");
    return CHANNEL_DELETED;
  }
  switch (opcode) {
    case WebSocketFrameHeader::kOpCodeText:  // fall-thru
    case WebSocketFrameHeader::kOpCodeBinary:
    case WebSocketFrameHeader::kOpCodeContinuation:
      return HandleDataFrame(opcode, final, std::move(payload));

    case WebSocketFrameHeader::kOpCodePing:
      DVLOG(1) << "Got Ping of size " << payload.size();
      if (state_ == CONNECTED) {
        auto buffer = base::MakeRefCounted<IOBufferWithSize>(payload.size());
        base::ranges::copy(payload, buffer->data());
        return SendFrameInternal(true, WebSocketFrameHeader::kOpCodePong,
                                 std::move(buffer), payload.size());
      }
      DVLOG(3) << "Ignored ping in state " << state_;
      return CHANNEL_ALIVE;

    case WebSocketFrameHeader::kOpCodePong:
      DVLOG(1) << "Got Pong of size " << payload.size();
      // There is no need to do anything with pong messages.
      return CHANNEL_ALIVE;

    case WebSocketFrameHeader::kOpCodeClose: {
      uint16_t code = kWebSocketNormalClosure;
      std::string reason;
      std::string message;
      if (!ParseClose(payload, &code, &reason, &message)) {
        FailChannel(message, code, reason);
        return CHANNEL_DELETED;
      }
      // TODO(ricea): Find a way to safely log the message from the close
      // message (escape control codes and so on).
      return HandleCloseFrame(code, reason);
    }

    default:
      FailChannel(base::StringPrintf("Unrecognized frame opcode: %d", opcode),
                  kWebSocketErrorProtocolError, "Unknown opcode");
      return CHANNEL_DELETED;
  }
}

ChannelState WebSocketChannel::HandleDataFrame(
    WebSocketFrameHeader::OpCode opcode,
    bool final,
    base::span<const char> payload) {
  DVLOG(3) << "WebSocketChannel::HandleDataFrame opcode=" << opcode
           << ", final?" << final << ", data=" << (void*)payload.data()
           << ", size=" << payload.size();
  if (state_ != CONNECTED) {
    DVLOG(3) << "Ignored data packet received in state " << state_;
    return CHANNEL_ALIVE;
  }
  if (has_received_close_frame_) {
    DVLOG(3) << "Ignored data packet as we've received a close frame.";
    return CHANNEL_ALIVE;
  }
  DCHECK(opcode == WebSocketFrameHeader::kOpCodeContinuation ||
         opcode == WebSocketFrameHeader::kOpCodeText ||
         opcode == WebSocketFrameHeader::kOpCodeBinary);
  const bool got_continuation =
      (opcode == WebSocketFrameHeader::kOpCodeContinuation);
  if (got_continuation != expecting_to_handle_continuation_) {
    const std::string console_log = got_continuation
        ? "Received unexpected continuation frame."
        : "Received start of new message but previous message is unfinished.";
    const std::string reason = got_continuation
        ? "Unexpected continuation"
        : "Previous data frame unfinished";
    FailChannel(console_log, kWebSocketErrorProtocolError, reason);
    return CHANNEL_DELETED;
  }
  expecting_to_handle_continuation_ = !final;
  WebSocketFrameHeader::OpCode opcode_to_send = opcode;
  if (!initial_frame_forwarded_ &&
      opcode == WebSocketFrameHeader::kOpCodeContinuation) {
    opcode_to_send = receiving_text_message_
                         ? WebSocketFrameHeader::kOpCodeText
                         : WebSocketFrameHeader::kOpCodeBinary;
  }
  if (opcode == WebSocketFrameHeader::kOpCodeText ||
      (opcode == WebSocketFrameHeader::kOpCodeContinuation &&
       receiving_text_message_)) {
    // This call is not redundant when size == 0 because it tells us what
    // the current state is.
    StreamingUtf8Validator::State state =
        incoming_utf8_validator_.AddBytes(base::as_byte_span(payload));
    if (state == StreamingUtf8Validator::INVALID ||
        (state == StreamingUtf8Validator::VALID_MIDPOINT && final)) {
      FailChannel("Could not decode a text frame as UTF-8.",
                  kWebSocketErrorProtocolError, "Invalid UTF-8 in text frame");
      return CHANNEL_DELETED;
    }
    receiving_text_message_ = !final;
    DCHECK(!final || state == StreamingUtf8Validator::VALID_ENDPOINT);
  }
  if (payload.size() == 0U && !final)
    return CHANNEL_ALIVE;

  initial_frame_forwarded_ = !final;
  // Sends the received frame to the renderer process.
  event_interface_->OnDataFrame(final, opcode_to_send, payload);
  return CHANNEL_ALIVE;
}

ChannelState WebSocketChannel::HandleCloseFrame(uint16_t code,
                                                const std::string& reason) {
  DVLOG(1) << "Got Close with code " << code;
  switch (state_) {
    case CONNECTED:
      has_received_close_frame_ = true;
      received_close_code_ = code;
      received_close_reason_ = reason;
      if (event_interface_->HasPendingDataFrames()) {
        // We have some data to be sent to the renderer before sending this
        // frame.
        return CHANNEL_ALIVE;
      }
      return RespondToClosingHandshake();

    case SEND_CLOSED:
      SetState(CLOSE_WAIT);
      DCHECK(close_timer_.IsRunning());
      close_timer_.Stop();
      // This use of base::Unretained() is safe because we stop the timer
      // in the destructor.
      close_timer_.Start(FROM_HERE, underlying_connection_close_timeout_,
                         base::BindOnce(&WebSocketChannel::CloseTimeout,
                                        base::Unretained(this)));

      // From RFC6455 section 7.1.5: "Each endpoint
      // will see the status code sent by the other end as _The WebSocket
      // Connection Close Code_."
      has_received_close_frame_ = true;
      received_close_code_ = code;
      received_close_reason_ = reason;
      break;

    default:
      LOG(DFATAL) << "Got Close in unexpected state " << state_;
      break;
  }
  return CHANNEL_ALIVE;
}

ChannelState WebSocketChannel::RespondToClosingHandshake() {
  DCHECK(has_received_close_frame_);
  DCHECK_EQ(CONNECTED, state_);
  SetState(RECV_CLOSED);
  if (SendClose(received_close_code_, received_close_reason_) ==
      CHANNEL_DELETED)
    return CHANNEL_DELETED;
  DCHECK_EQ(RECV_CLOSED, state_);

  SetState(CLOSE_WAIT);
  DCHECK(!close_timer_.IsRunning());
  // This use of base::Unretained() is safe because we stop the timer
  // in the destructor.
  close_timer_.Start(
      FROM_HERE, underlying_connection_close_timeout_,
      base::BindOnce(&WebSocketChannel::CloseTimeout, base::Unretained(this)));

  event_interface_->OnClosingHandshake();
  return CHANNEL_ALIVE;
}

ChannelState WebSocketChannel::SendFrameInternal(
    bool fin,
    WebSocketFrameHeader::OpCode op_code,
    scoped_refptr<IOBuffer> buffer,
    uint64_t buffer_size) {
  DCHECK(state_ == CONNECTED || state_ == RECV_CLOSED);
  DCHECK(stream_);

  auto frame = std::make_unique<WebSocketFrame>(op_code);
  WebSocketFrameHeader& header = frame->header;
  header.final = fin;
  header.masked = true;
  header.payload_length = buffer_size;
  frame->payload = buffer->data();

  if (data_being_sent_) {
    // Either the link to the WebSocket server is saturated, or several messages
    // are being sent in a batch.
    if (!data_to_send_next_)
      data_to_send_next_ = std::make_unique<SendBuffer>();
    data_to_send_next_->AddFrame(std::move(frame), std::move(buffer));
    return CHANNEL_ALIVE;
  }

  data_being_sent_ = std::make_unique<SendBuffer>();
  data_being_sent_->AddFrame(std::move(frame), std::move(buffer));
  return WriteFrames();
}

void WebSocketChannel::FailChannel(const std::string& message,
                                   uint16_t code,
                                   const std::string& reason) {
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  DCHECK_NE(CLOSED, state_);

  stream_->GetNetLogWithSource().AddEvent(
      net::NetLogEventType::WEBSOCKET_INVALID_FRAME,
      [&] { return NetLogFailParam(code, reason, message); });

  if (state_ == CONNECTED) {
    if (SendClose(code, reason) == CHANNEL_DELETED)
      return;
  }

  // Careful study of RFC6455 section 7.1.7 and 7.1.1 indicates the browser
  // should close the connection itself without waiting for the closing
  // handshake.
  stream_->Close();
  SetState(CLOSED);
  event_interface_->OnFailChannel(message, ERR_FAILED, std::nullopt);
}

ChannelState WebSocketChannel::SendClose(uint16_t code,
                                         const std::string& reason) {
  DCHECK(state_ == CONNECTED || state_ == RECV_CLOSED);
  DCHECK_LE(reason.size(), kMaximumCloseReasonLength);
  scoped_refptr<IOBuffer> body;
  uint64_t size = 0;
  if (code == kWebSocketErrorNoStatusReceived) {
    // Special case: translate kWebSocketErrorNoStatusReceived into a Close
    // frame with no payload.
    DCHECK(reason.empty());
    body = base::MakeRefCounted<IOBufferWithSize>();
  } else {
    const size_t payload_length = kWebSocketCloseCodeLength + reason.length();
    body = base::MakeRefCounted<IOBufferWithSize>(payload_length);
    size = payload_length;
    auto [code_span, body_span] =
        body->span().split_at<kWebSocketCloseCodeLength>();
    code_span.copy_from(base::U16ToBigEndian(code));
    static_assert(sizeof(code) == kWebSocketCloseCodeLength,
                  "they should both be two");
    body_span.copy_from(base::as_byte_span(reason));
  }

  return SendFrameInternal(true, WebSocketFrameHeader::kOpCodeClose,
                           std::move(body), size);
}

bool WebSocketChannel::ParseClose(base::span<const char> payload,
                                  uint16_t* code,
                                  std::string* reason,
                                  std::string* message) {
  const uint64_t size = static_cast<uint64_t>(payload.size());
  reason->clear();
  if (size < kWebSocketCloseCodeLength) {
    if (size == 0U) {
      *code = kWebSocketErrorNoStatusReceived;
      return true;
    }

    DVLOG(1) << "Close frame with payload size " << size << " received "
             << "(the first byte is " << std::hex
             << static_cast<int>(payload[0]) << ")";
    *code = kWebSocketErrorProtocolError;
    *message =
        "Received a broken close frame containing an invalid size body.";
    return false;
  }

  const char* data = payload.data();
  uint16_t unchecked_code =
      base::U16FromBigEndian(base::as_byte_span(payload).first<2>());
  static_assert(sizeof(unchecked_code) == kWebSocketCloseCodeLength,
                "they should both be two bytes");

  switch (unchecked_code) {
    case kWebSocketErrorNoStatusReceived:
    case kWebSocketErrorAbnormalClosure:
    case kWebSocketErrorTlsHandshake:
      *code = kWebSocketErrorProtocolError;
      *message =
          "Received a broken close frame containing a reserved status code.";
      return false;

    default:
      *code = unchecked_code;
      break;
  }

  std::string text(data + kWebSocketCloseCodeLength, data + size);
  if (StreamingUtf8Validator::Validate(text)) {
    reason->swap(text);
    return true;
  }

  *code = kWebSocketErrorProtocolError;
  *reason = "Invalid UTF-8 in Close frame";
  *message = "Received a broken close frame containing invalid UTF-8.";
  return false;
}

void WebSocketChannel::DoDropChannel(bool was_clean,
                                     uint16_t code,
                                     const std::string& reason) {
  event_interface_->OnDropChannel(was_clean, code, reason);
}

void WebSocketChannel::CloseTimeout() {
  stream_->GetNetLogWithSource().AddEvent(
      net::NetLogEventType::WEBSOCKET_CLOSE_TIMEOUT);
  stream_->Close();
  SetState(CLOSED);
  if (has_received_close_frame_) {
    DoDropChannel(true, received_close_code_, received_close_reason_);
  } else {
    DoDropChannel(false, kWebSocketErrorAbnormalClosure, "");
  }
  // |this| has been deleted.
}

}  // namespace net
