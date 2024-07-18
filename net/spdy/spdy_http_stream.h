// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HTTP_STREAM_H_
#define NET_SPDY_SPDY_HTTP_STREAM_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/log/net_log_source.h"
#include "net/spdy/multiplexed_http_stream.h"
#include "net/spdy/spdy_read_queue.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"

namespace net {

struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
class SpdySession;
class UploadDataStream;

// The SpdyHttpStream is a HTTP-specific type of stream known to a SpdySession.
class NET_EXPORT_PRIVATE SpdyHttpStream : public SpdyStream::Delegate,
                                          public MultiplexedHttpStream {
 public:
  static const size_t kRequestBodyBufferSize;
  // |spdy_session| must not be NULL.
  SpdyHttpStream(const base::WeakPtr<SpdySession>& spdy_session,
                 NetLogSource source_dependency,
                 std::set<std::string> dns_aliases);

  SpdyHttpStream(const SpdyHttpStream&) = delete;
  SpdyHttpStream& operator=(const SpdyHttpStream&) = delete;

  ~SpdyHttpStream() override;

  SpdyStream* stream() { return stream_; }

  // Cancels any callbacks from being invoked and deletes the stream.
  void Cancel();

  // HttpStream implementation.
  void RegisterRequest(const HttpRequestInfo* request_info) override;
  int InitializeStream(bool can_send_early,
                       RequestPriority priority,
                       const NetLogWithSource& net_log,
                       CompletionOnceCallback callback) override;

  int SendRequest(const HttpRequestHeaders& headers,
                  HttpResponseInfo* response,
                  CompletionOnceCallback callback) override;
  int ReadResponseHeaders(CompletionOnceCallback callback) override;
  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) override;
  void Close(bool not_reusable) override;
  bool IsResponseBodyComplete() const override;

  // Must not be called if a NULL SpdySession was pssed into the
  // constructor.
  bool IsConnectionReused() const override;

  // Total number of bytes received over the network of SPDY data, headers, and
  // push_promise frames associated with this stream, including the size of
  // frame headers, after SSL decryption and not including proxy overhead.
  int64_t GetTotalReceivedBytes() const override;
  // Total number of bytes sent over the network of SPDY frames associated with
  // this stream, including the size of frame headers, before SSL encryption and
  // not including proxy overhead. Note that some SPDY frames such as pings are
  // not associated with any stream, and are not included in this value.
  int64_t GetTotalSentBytes() const override;
  bool GetAlternativeService(
      AlternativeService* alternative_service) const override;
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  int GetRemoteEndpoint(IPEndPoint* endpoint) override;
  void PopulateNetErrorDetails(NetErrorDetails* details) override;
  void SetPriority(RequestPriority priority) override;
  const std::set<std::string>& GetDnsAliases() const override;
  std::string_view GetAcceptChViaAlps() const override;

  // SpdyStream::Delegate implementation.
  void OnHeadersSent() override;
  void OnEarlyHintsReceived(const quiche::HttpHeaderBlock& headers) override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override;
  void OnDataSent() override;
  void OnTrailers(const quiche::HttpHeaderBlock& trailers) override;
  void OnClose(int status) override;
  bool CanGreaseFrameType() const override;
  NetLogSource source_dependency() const override;

 private:
  // Helper function used to initialize private members and to set delegate on
  // stream when stream is created.
  void InitializeStreamHelper();

  // Helper function used for resetting stream from inside the stream.
  void ResetStream(int error);

  // Must be called only when |request_info_| is non-NULL.
  bool HasUploadData() const;

  void OnStreamCreated(CompletionOnceCallback callback, int rv);

  // Reads the remaining data (whether chunked or not) from the
  // request body stream and sends it if there's any. The read and
  // subsequent sending may happen asynchronously. Must be called only
  // when HasUploadData() is true.
  void ReadAndSendRequestBodyData();

  // Send an empty body.  Must only be called if there is no upload data and
  // sending greased HTTP/2 frames is enabled.  This allows SpdyStream to
  // prepend a greased HTTP/2 frame to the empty DATA frame that closes the
  // stream.
  void SendEmptyBody();

  // Called when data has just been read from the request body stream;
  // does the actual sending of data.
  void OnRequestBodyReadCompleted(int status);

  // Call the user callback associated with sending the request.
  void DoRequestCallback(int rv);

  // Method to PostTask for calling request callback asynchronously.
  void MaybeDoRequestCallback(int rv);

  // Post the request callback if not null.
  // This is necessary because the request callback might destroy |stream_|,
  // which does not support that.
  void MaybePostRequestCallback(int rv);

  // Call the user callback associated with reading the response.
  void DoResponseCallback(int rv);

  void MaybeScheduleBufferedReadCallback();
  void DoBufferedReadCallback();

  const base::WeakPtr<SpdySession> spdy_session_;

  bool is_reused_;
  SpdyStreamRequest stream_request_;
  const NetLogSource source_dependency_;

  // |stream_| is owned by SpdySession.
  // Before InitializeStream() is called, stream_ == nullptr.
  // After InitializeStream() is called but before OnClose() is called,
  //   |*stream_| is guaranteed to be valid.
  // After OnClose() is called, stream_ == nullptr.
  raw_ptr<SpdyStream> stream_ = nullptr;

  // False before OnClose() is called, true after.
  bool stream_closed_ = false;

  // Set only when |stream_closed_| is true.
  int closed_stream_status_ = ERR_FAILED;
  spdy::SpdyStreamId closed_stream_id_ = 0;
  bool closed_stream_has_load_timing_info_;
  LoadTimingInfo closed_stream_load_timing_info_;
  // After |stream_| has been closed, this keeps track of the total number of
  // bytes received over the network for |stream_| while it was open.
  int64_t closed_stream_received_bytes_ = 0;
  // After |stream_| has been closed, this keeps track of the total number of
  // bytes sent over the network for |stream_| while it was open.
  int64_t closed_stream_sent_bytes_ = 0;

  // The request to send.
  // Set to null before response body is starting to be read. This is to allow
  // |this| to be shared for reading and to possibly outlive request_info_'s
  // owner. Setting to null happens after headers are completely read or upload
  // data stream is uploaded, whichever is later.
  raw_ptr<const HttpRequestInfo> request_info_ = nullptr;

  // |response_info_| is the HTTP response data object which is filled in
  // when a response HEADERS comes in for the stream.
  // It is not owned by this stream object.
  raw_ptr<HttpResponseInfo> response_info_ = nullptr;

  bool response_headers_complete_ = false;

  bool upload_stream_in_progress_ = false;

  // We buffer the response body as it arrives asynchronously from the stream.
  SpdyReadQueue response_body_queue_;

  CompletionOnceCallback request_callback_;
  CompletionOnceCallback response_callback_;

  // User provided buffer for the ReadResponseBody() response.
  scoped_refptr<IOBuffer> user_buffer_;
  int user_buffer_len_ = 0;

  // Temporary buffer used to read the request body from UploadDataStream.
  scoped_refptr<IOBufferWithSize> request_body_buf_;
  int request_body_buf_size_ = 0;

  // Timer to execute DoBufferedReadCallback() with a delay.
  base::OneShotTimer buffered_read_timer_;

  // Stores any DNS aliases for the remote endpoint. Includes all known aliases,
  // e.g. from A, AAAA, or HTTPS, not just from the address used for the
  // connection, in no particular order. These are stored in the stream instead
  // of the session due to complications related to IP-pooling.
  std::set<std::string> dns_aliases_;

  // Keep track of the priority of the request for setting the priority header
  // right before sending the request.
  RequestPriority priority_ = RequestPriority::DEFAULT_PRIORITY;

  base::WeakPtrFactory<SpdyHttpStream> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SPDY_SPDY_HTTP_STREAM_H_
