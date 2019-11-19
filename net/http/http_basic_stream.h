// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/http/http_basic_state.h"
#include "net/http/http_stream.h"

namespace net {

class ClientSocketHandle;
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
  HttpBasicStream(std::unique_ptr<ClientSocketHandle> connection,
                  bool using_proxy);
  ~HttpBasicStream() override;

  // HttpStream methods:
  int InitializeStream(const HttpRequestInfo* request_info,
                       bool can_send_early,
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

  HttpStream* RenewStreamForAuth() override;

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

  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override;

  bool GetRemoteEndpoint(IPEndPoint* endpoint) override;

  void Drain(HttpNetworkSession* session) override;

  void PopulateNetErrorDetails(NetErrorDetails* details) override;

  void SetPriority(RequestPriority priority) override;

  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;

 private:
  HttpStreamParser* parser() const { return state_.parser(); }

  void OnHandshakeConfirmed(CompletionOnceCallback callback, int rv);

  HttpBasicState state_;
  base::TimeTicks confirm_handshake_end_;
  RequestHeadersCallback request_headers_callback_;

  DISALLOW_COPY_AND_ASSIGN(HttpBasicStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_BASIC_STREAM_H_
