// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "build/build_config.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/tools/quic/quic_http_proxy_backend.h"
#include "net/tools/quic/quic_http_proxy_backend_stream.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_interceptor.h"

namespace net {

QuicHttpProxyBackend::QuicHttpProxyBackend()
    : context_(nullptr), thread_initialized_(false), proxy_thread_(nullptr) {}

QuicHttpProxyBackend::~QuicHttpProxyBackend() {
  backend_stream_map_.clear();
  thread_initialized_ = false;
  proxy_task_runner_->DeleteSoon(FROM_HERE, context_.release());
  if (proxy_thread_ != nullptr) {
    LOG(INFO) << "QUIC Proxy thread: " << proxy_thread_->thread_name()
              << " has stopped !";
    proxy_thread_.reset();
  }
}

bool QuicHttpProxyBackend::InitializeBackend(const std::string& backend_url) {
  if (!ValidateBackendUrl(backend_url)) {
    return false;
  }
  if (proxy_thread_ == nullptr) {
    proxy_thread_ = std::make_unique<base::Thread>("quic proxy thread");
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    bool result = proxy_thread_->StartWithOptions(options);
    proxy_task_runner_ = proxy_thread_->task_runner();
    CHECK(result);
  }
  thread_initialized_ = true;
  return true;
}

bool QuicHttpProxyBackend::ValidateBackendUrl(const std::string& backend_url) {
  backend_url_ = GURL(backend_url);
  // Only Http(s) backend supported
  if (!backend_url_.is_valid() || !backend_url_.SchemeIsHTTPOrHTTPS()) {
    LOG(ERROR) << "QUIC Proxy Backend URL '" << backend_url
               << "' is not valid !";
    return false;
  }

  LOG(INFO)
      << "Successfully configured to run as a QUIC Proxy with Backend URL: "
      << backend_url_.spec();
  return true;
}

bool QuicHttpProxyBackend::IsBackendInitialized() const {
  return thread_initialized_;
}

void QuicHttpProxyBackend::FetchResponseFromBackend(
    const spdy::SpdyHeaderBlock& request_headers,
    const std::string& incoming_body,
    QuicSimpleServerBackend::RequestHandler* quic_server_stream) {
  QuicHttpProxyBackendStream* proxy_backend_stream =
      InitializeQuicProxyBackendStream(quic_server_stream);

  LOG(INFO) << " Forwarding QUIC request to the Backend Thread Asynchronously.";
  if (proxy_backend_stream == nullptr ||
      proxy_backend_stream->SendRequestToBackend(&request_headers,
                                                 incoming_body) != true) {
    std::list<quic::QuicBackendResponse::ServerPushInfo> empty_resources;
    quic_server_stream->OnResponseBackendComplete(nullptr, empty_resources);
  }
}

QuicHttpProxyBackendStream*
QuicHttpProxyBackend::InitializeQuicProxyBackendStream(
    QuicSimpleServerBackend::RequestHandler* quic_server_stream) {
  if (!thread_initialized_) {
    return nullptr;
  }
  QuicHttpProxyBackendStream* proxy_backend_stream =
      new QuicHttpProxyBackendStream(this);
  proxy_backend_stream->set_delegate(quic_server_stream);
  proxy_backend_stream->Initialize(quic_server_stream->connection_id(),
                                   quic_server_stream->stream_id(),
                                   quic_server_stream->peer_host());
  {
    // Aquire write lock for this scope
    base::AutoLock lock(backend_stream_mutex_);

    auto inserted = backend_stream_map_.insert(std::make_pair(
        quic_server_stream, base::WrapUnique(proxy_backend_stream)));
    DCHECK(inserted.second);
  }
  return proxy_backend_stream;
}

void QuicHttpProxyBackend::CloseBackendResponseStream(
    QuicSimpleServerBackend::RequestHandler* quic_server_stream) {
  // Clean close of the backend stream handler
  if (quic_server_stream == nullptr) {
    return;
  }
  // Cleanup the handler on the proxy thread, since it owns the url_request
  if (!proxy_task_runner_->BelongsToCurrentThread()) {
    proxy_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&QuicHttpProxyBackend::CloseBackendResponseStream,
                       base::Unretained(this), quic_server_stream));
  } else {
    // Aquire write lock for this scope and cancel if the request is still
    // pending
    base::AutoLock lock(backend_stream_mutex_);
    QuicHttpProxyBackendStream* proxy_backend_stream = nullptr;

    auto it = backend_stream_map_.find(quic_server_stream);
    if (it != backend_stream_map_.end()) {
      proxy_backend_stream = it->second.get();
      proxy_backend_stream->CancelRequest();
      proxy_backend_stream->reset_delegate();
      LOG(INFO) << " Quic Proxy cleaned-up backend handler on context/main "
                   "thread for quic_conn_id: "
                << proxy_backend_stream->quic_connection_id()
                << " quic_stream_id: "
                << proxy_backend_stream->quic_stream_id();
      size_t erased = backend_stream_map_.erase(quic_server_stream);
      DCHECK_EQ(1u, erased);
    }
  }
}

void QuicHttpProxyBackend::InitializeURLRequestContext() {
  DCHECK(context_ == nullptr);
  net::URLRequestContextBuilder context_builder;
  // Quic reverse proxy does not cache HTTP objects
  context_builder.DisableHttpCache();
  // Enable HTTP2, but disable QUIC on the backend
  context_builder.SetSpdyAndQuicEnabled(true /* http2 */, false /* quic */);

#if defined(OS_LINUX)
  // On Linux, use a fixed ProxyConfigService, since the default one
  // depends on glib.
  context_builder.set_proxy_config_service(
      std::make_unique<ProxyConfigServiceFixed>(
          ProxyConfigWithAnnotation::CreateDirect()));
#endif

  // Disable net::CookieStore.
  context_builder.SetCookieStore(nullptr);
  context_ = context_builder.Build();
}

net::URLRequestContext* QuicHttpProxyBackend::GetURLRequestContext() {
  // Access to URLRequestContext is only available on Backend Thread
  DCHECK(proxy_task_runner_->BelongsToCurrentThread());
  if (context_ == nullptr) {
    InitializeURLRequestContext();
  }
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
QuicHttpProxyBackend::GetProxyTaskRunner() const {
  return proxy_task_runner_;
}

}  // namespace net
