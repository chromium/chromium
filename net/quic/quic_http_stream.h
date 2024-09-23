// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_HTTP_STREAM_H_
#define NET_QUIC_QUIC_HTTP_STREAM_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/idempotency.h"
#include "net/base/io_buffer.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_client_stream.h"
#include "net/spdy/multiplexed_http_stream.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"

namespace net {

namespace test {
class QuicHttpStreamPeer;
}  // namespace test

// The QuicHttpStream is a QUIC-specific HttpStream subclass.  It holds a
// handle of QuicChromiumClientStream which it uses to send and receive data.
// The handle hides the details of the underlying stream's lifetime and can be
// used even after the underlying stream is destroyed.
class NET_EXPORT_PRIVATE QuicHttpStream : public MultiplexedHttpStream {
 public:
  explicit QuicHttpStream(
      std::unique_ptr<QuicChromiumClientSession::Handle> session,
      std::set<std::string> dns_aliases);

  QuicHttpStream(const QuicHttpStream&) = delete;
  QuicHttpStream& operator=(const QuicHttpStream&) = delete;

  ~QuicHttpStream() override;

  // HttpStream implementation.
  void RegisterRequest(const HttpRequestInfo* request_info) override;
  int InitializeStream(bool can_send_early,
                       RequestPriority priority,
                       const NetLogWithSource& net_log,
                       CompletionOnceCallback callback) override;
  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  CompletionOnceCallback callback) override;
  int ReadResponseHeaders(CompletionOnceCallback callback) override;
  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) override;
  void Close(bool not_reusable) override;
  bool IsResponseBodyComplete() const override;
  bool IsConnectionReused() const override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  bool GetAlternativeService(
      AlternativeService* alternative_service) const override;
  void PopulateNetErrorDetails(NetErrorDetails* details) override;
  void SetPriority(RequestPriority priority) override;
  void SetRequestIdempotency(Idempotency idempotency) override;
  const std::set<std::string>& GetDnsAliases() const override;
  std::string_view GetAcceptChViaAlps() const override;
  std::optional<QuicErrorDetails> GetQuicErrorDetails() const override;

  static HttpConnectionInfo ConnectionInfoFromQuicVersion(
      quic::ParsedQuicVersion quic_version);

 private:
  friend class test::QuicHttpStreamPeer;

  enum State {
    STATE_NONE,
    STATE_REQUEST_STREAM,
    STATE_REQUEST_STREAM_COMPLETE,
    STATE_SET_REQUEST_PRIORITY,
    STATE_SEND_HEADERS,
    STATE_SEND_HEADERS_COMPLETE,
    STATE_READ_REQUEST_BODY,
    STATE_READ_REQUEST_BODY_COMPLETE,
    STATE_SEND_BODY,
    STATE_SEND_BODY_COMPLETE,
    STATE_OPEN,
  };

  void OnIOComplete(int rv);
  void DoCallback(int rv);

  int DoLoop(int rv);
  int DoRequestStream();
  int DoRequestStreamComplete(int rv);
  int DoSetRequestPriority();
  int DoSendHeaders();
  int DoSendHeadersComplete(int rv);
  int DoReadRequestBody();
  int DoReadRequestBodyComplete(int rv);
  int DoSendBody();
  int DoSendBodyComplete(int rv);

  void OnReadResponseHeadersComplete(int rv);
  int ProcessResponseHeaders(const quiche::HttpHeaderBlock& headers);
  void ReadTrailingHeaders();
  void OnReadTrailingHeadersComplete(int rv);

  void OnReadBodyComplete(int rv);
  int HandleReadComplete(int rv);

  void EnterStateSendHeaders();

  void ResetStream();

  // Returns ERR_QUIC_HANDSHAKE_FAILED, if |rv| is ERR_QUIC_PROTOCOL_ERROR and
  // the handshake was never confirmed. Otherwise, returns |rv|.
  int MapStreamError(int rv);

  // If |has_response_status_| is false, sets |response_status| to the result
  // of ComputeResponseStatus(). Returns |response_status_|.
  int GetResponseStatus();
  // Sets the result of |ComputeResponseStatus()| as the |response_status_|.
  void SaveResponseStatus();
  // Sets |response_status_| to |response_status| and sets
  // |has_response_status_| to true.
  void SetResponseStatus(int response_status);
  // Computes the correct response status based on the status of the handshake,
  // |session_error|, |connection_error| and |stream_error|.
  int ComputeResponseStatus() const;

  QuicChromiumClientSession::Handle* quic_session() {
    return static_cast<QuicChromiumClientSession::Handle*>(session());
  }

  const QuicChromiumClientSession::Handle* quic_session() const {
    return static_cast<const QuicChromiumClientSession::Handle*>(session());
  }

  State next_state_ = STATE_NONE;

  std::unique_ptr<QuicChromiumClientStream::Handle> stream_;

  // The following three fields are all owned by the caller and must
  // outlive this object, according to the HttpStream contract.

  // The request to send.
  // Only valid before the response body is read.
  raw_ptr<const HttpRequestInfo> request_info_ = nullptr;

  // Whether this request can be sent without confirmation.
  bool can_send_early_ = false;

  // The request body to send, if any, owned by the caller.
  raw_ptr<UploadDataStream> request_body_stream_ = nullptr;
  // Time the request was issued.
  base::Time request_time_;
  // The priority of the request.
  RequestPriority priority_ = MINIMUM_PRIORITY;
  // |response_info_| is the HTTP response data object which is filled in
  // when a the response headers are read.  It is not owned by this stream.
  raw_ptr<HttpResponseInfo> response_info_ = nullptr;
  bool has_response_status_ = false;  // true if response_status_ as been set.
  // Because response data is buffered, also buffer the response status if the
  // stream is explicitly closed via OnError or OnClose with an error.
  // Once all buffered data has been returned, this will be used as the final
  // response.
  int response_status_ = ERR_UNEXPECTED;

  // Serialized request headers.
  quiche::HttpHeaderBlock request_headers_;

  quiche::HttpHeaderBlock response_header_block_;
  bool response_headers_received_ = false;

  quiche::HttpHeaderBlock trailing_header_block_;
  bool trailing_headers_received_ = false;

  // Number of bytes received by the headers stream on behalf of this stream.
  int64_t headers_bytes_received_ = 0;
  // Number of bytes sent by the headers stream on behalf of this stream.
  int64_t headers_bytes_sent_ = 0;

  // Number of bytes received when the stream was closed.
  int64_t closed_stream_received_bytes_ = 0;
  // Number of bytes sent when the stream was closed.
  int64_t closed_stream_sent_bytes_ = 0;
  // True if the stream is the first stream negotiated on the session. Set when
  // the stream was closed. If |stream_| is failed to be created, this takes on
  // the default value of false.
  bool closed_is_first_stream_ = false;

  quic::QuicErrorCode connection_error_ = quic::QUIC_NO_ERROR;
  quic::QuicRstStreamErrorCode stream_error_ = quic::QUIC_STREAM_NO_ERROR;
  uint64_t connection_wire_error_ = 0;
  uint64_t ietf_application_error_ = 0;

  // The caller's callback to be used for asynchronous operations.
  CompletionOnceCallback callback_;

  // Caller provided buffer for the ReadResponseBody() response.
  scoped_refptr<IOBuffer> user_buffer_;
  int user_buffer_len_ = 0;

  // Temporary buffer used to read the request body from UploadDataStream.
  scoped_refptr<IOBufferWithSize> raw_request_body_buf_;
  // Wraps raw_request_body_buf_ to read the remaining data progressively.
  scoped_refptr<DrainableIOBuffer> request_body_buf_;

  NetLogWithSource stream_net_log_;

  int session_error_ =
      ERR_UNEXPECTED;  // Error code from the connection shutdown.

  // Set to true when DoLoop() is being executed, false otherwise.
  bool in_loop_ = false;

  // Session connect timing info.
  LoadTimingInfo::ConnectTiming connect_timing_;

  // Stores any DNS aliases for the remote endpoint. Includes all known
  // aliases, e.g. from A, AAAA, or HTTPS, not just from the address used for
  // the connection, in no particular order. These are stored in the stream
  // instead of the session due to complications related to IP-pooling.
  std::set<std::string> dns_aliases_;

  base::WeakPtrFactory<QuicHttpStream> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_HTTP_STREAM_H_
