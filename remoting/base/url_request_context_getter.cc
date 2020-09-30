// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/url_request_context_getter.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "remoting/base/vlog_net_log.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "net/log/net_log.h"
#endif  // defined(OS_WIN)

namespace remoting {

URLRequestContextGetter::URLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : network_task_runner_(network_task_runner),
      proxy_config_service_(
          net::ConfiguredProxyResolutionService::CreateSystemProxyConfigService(
              network_task_runner)) {}

net::URLRequestContext* URLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_.get()) {
    CreateVlogNetLogObserver();
    net::URLRequestContextBuilder builder;
    builder.DisableHttpCache();

#if defined(OS_WIN)
    if (base::win::GetVersion() <= base::win::Version::WIN7) {
      // The network stack of Windows 7 and older systems has a bug such that
      // proxy resolution always fails and blocks each request for ~10-30
      // seconds. We don't support proxied connection right now, so just disable
      // it on Windows 7 HTTP requests.
      auto proxy_resolution_service =
          net::ConfiguredProxyResolutionService::CreateWithoutProxyResolver(
              std::move(proxy_config_service_), net::NetLog::Get());
      builder.set_proxy_resolution_service(std::move(proxy_resolution_service));
    }
#endif  // defined(OS_WIN)

    if (proxy_config_service_) {
      builder.set_proxy_config_service(std::move(proxy_config_service_));
    }
    url_request_context_ = builder.Build();
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
URLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

URLRequestContextGetter::~URLRequestContextGetter() = default;

}  // namespace remoting
