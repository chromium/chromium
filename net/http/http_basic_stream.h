// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpBasicStream is a simple implementation of HttpStream.  It assumes it is
// not sharing a sharing with any other HttpStreams, therefore it just reads and
// writes directly to the Http Stream.

#ifndef NET_HTTP_HTTP_BASIC_STREAM_H_
#define NET_HTTP_HTTP_BASIC_STREAM_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_basic_state.h"
#include "net/http/http_stream.h"

namespace net {

class StreamSocketHandle;
class HttpResponseInfo;
struct HttpRequestInfo;
class HttpRequestHeaders;
class HttpStreamParser;
class IOBuffer;
class NetLogWithSource;

class NET_EXPORT_PRIVATE HttpBasicStream : public HttpStream {
 public:
  // Constructs a new HttpBasicStream. InitializeStream must be called to
  // initialize it correctly.
  HttpBasicStream(std::unique_ptr<StreamSocketHandle> connection,
                  bool is_for_get_to_http_proxy);

  HttpBasicStream(const HttpBasicStream&) = delete;
  HttpBasicStream& operator=(const HttpBasicStream&) = delete;

  ~HttpBasicStream() override;

  // HttpStream methods:
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

  std::unique_ptr<HttpStream> RenewStreamForAuth() override;

  bool IsResponseBodyComplete() const override;

  bool IsConnectionReused() const override;

  void SetConnectionReused() override;

  bool CanReuseConnection() const override;

  int64_t GetTotalReceivedBytes() const override;

  int64_t GetTotalSentBytes() const override;

  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;

  bool GetAlternativeService(
      AlternativeService* alternative_service) const override;

  void GetSSLInfo(SSLInfo* ssl_info) override;

  int GetRemoteEndpoint(IPEndPoint* endpoint) override;

  void Drain(HttpNetworkSession* session) override;

  void PopulateNetErrorDetails(NetErrorDetails* details) override;

  void SetPriority(RequestPriority priority) override;

  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;

  const std::set<std::string>& GetDnsAliases() const override;

  std::string_view GetAcceptChViaAlps() const override;

 private:
  HttpStreamParser* parser() const { return state_.parser(); }

  void OnHandshakeConfirmed(CompletionOnceCallback callback, int rv);

  HttpBasicState state_;
  base::TimeTicks confirm_handshake_end_;
  RequestHeadersCallback request_headers_callback_;
  // The request to send.
  // Set to null before the response body is read. This is to allow |this| to
  // be shared for reading and to possibly outlive request_info_'s owner.
  // Setting to null happens after headers are completely read or upload data
  // stream is uploaded, whichever is later.
  raw_ptr<const HttpRequestInfo> request_info_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_BASIC_STREAM_H_
