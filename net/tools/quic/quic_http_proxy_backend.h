// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The proxy functionality is implemented as a separate thread namely
// “quic proxy thread”, managed by an instance of the QuicHttpProxyBackend
// class. The QuicHttpProxyBackend instance also manages an instance of the
// class net::URLRequestContext, that manages a single context for all the
// HTTP calls made to the backend server. Finally, the QuicHttpProxyBackend
// instance owns (creates/ destroys) the instances of QuicHttpProxyBackendStream
// to avoid orphan pointers of QuicHttpProxyBackendStream when the corresponding
// QUIC connection is destroyed on the main thread due to several reasons. The
// QUIC connection management and protocol parsing is performed by the main/quic
// thread, in the same way as the toy QUIC server.
//
// quic_http_proxy_backend_stream.h has a description of threads, the flow
// of packets in QUIC proxy in the forward and reverse directions.

#ifndef NET_TOOLS_QUIC_QUIC_HTTP_PROXY_BACKEND_H_
#define NET_TOOLS_QUIC_QUIC_HTTP_PROXY_BACKEND_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/base64.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_server_backend.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace quic {
class QuicSimpleServerBackend;
}  // namespace quic

namespace net {
class QuicHttpProxyBackendStream;

// Manages the context to proxy HTTP requests to the backend server
// Owns instance of net::URLRequestContext.
class QuicHttpProxyBackend : public quic::QuicSimpleServerBackend {
 public:
  explicit QuicHttpProxyBackend();
  ~QuicHttpProxyBackend() override;

  // Must be called from the backend thread of the quic proxy
  net::URLRequestContext* GetURLRequestContext();
  scoped_refptr<base::SingleThreadTaskRunner> GetProxyTaskRunner() const;

  using ProxyBackendStreamMap =
      std::unordered_map<quic::QuicSimpleServerBackend::RequestHandler*,
                         std::unique_ptr<QuicHttpProxyBackendStream>>;
  const ProxyBackendStreamMap* proxy_backend_streams_map() const {
    return &backend_stream_map_;
  }

  GURL backend_url() const { return backend_url_; }

  // Implements the functions for interface quic::QuicSimpleServerBackend
  bool InitializeBackend(const std::string& backend_url) override;
  bool IsBackendInitialized() const override;
  void FetchResponseFromBackend(
      const spdy::SpdyHeaderBlock& request_headers,
      const std::string& incoming_body,
      quic::QuicSimpleServerBackend::RequestHandler* quic_stream) override;
  void CloseBackendResponseStream(
      quic::QuicSimpleServerBackend::RequestHandler* quic_stream) override;

 private:
  // Maps quic streams in the frontend to the corresponding http streams
  // managed by |this|
  ProxyBackendStreamMap backend_stream_map_;

  bool ValidateBackendUrl(const std::string& backend_url);
  void InitializeURLRequestContext();
  QuicHttpProxyBackendStream* InitializeQuicProxyBackendStream(
      quic::QuicSimpleServerBackend::RequestHandler* quic_server_stream);

  // URLRequestContext to make URL requests to the backend
  std::unique_ptr<net::URLRequestContext> context_;  // owned by this

  bool thread_initialized_;
  // <scheme://hostname:port/ for the backend HTTP server
  GURL backend_url_;

  // Backend thread is owned by |this|
  std::unique_ptr<base::Thread> proxy_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner_;

  // Protects against concurrent access from quic (main) and proxy
  // threads for adding and clearing a backend request handler
  base::Lock backend_stream_mutex_;

  DISALLOW_COPY_AND_ASSIGN(QuicHttpProxyBackend);
};
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_HTTP_PROXY_BACKEND_H_
