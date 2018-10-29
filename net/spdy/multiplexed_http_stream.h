// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_MULTIPLEXED_HTTP_STREAM_H_
#define NET_SPDY_MULTIPLEXED_HTTP_STREAM_H_

#include <memory>
#include <vector>

#include "net/http/http_stream.h"
#include "net/spdy/multiplexed_session.h"

namespace spdy {
class SpdyHeaderBlock;
}  // namespace spdy
namespace net {

// Base class for SPDY and QUIC HttpStream subclasses.
class NET_EXPORT_PRIVATE MultiplexedHttpStream : public HttpStream {
 public:
  explicit MultiplexedHttpStream(
      std::unique_ptr<MultiplexedSessionHandle> session);
  ~MultiplexedHttpStream() override;

  bool GetRemoteEndpoint(IPEndPoint* endpoint) override;
  void GetSSLInfo(SSLInfo* ssl_info) override;
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override;
  void Drain(HttpNetworkSession* session) override;
  HttpStream* RenewStreamForAuth() override;
  void SetConnectionReused() override;
  bool CanReuseConnection() const override;

  // Caches SSL info from the underlying session.
  void SaveSSLInfo();
  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;

 protected:
  void DispatchRequestHeadersCallback(
      const spdy::SpdyHeaderBlock& spdy_headers);

  MultiplexedSessionHandle* session() { return session_.get(); }
  const MultiplexedSessionHandle* session() const { return session_.get(); }

 private:
  const std::unique_ptr<MultiplexedSessionHandle> session_;
  RequestHeadersCallback request_headers_callback_;
};

}  // namespace net

#endif  // NET_SPDY_MULTIPLEXED_HTTP_STREAM_H_
