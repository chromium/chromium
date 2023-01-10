// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/url_request_context_getter.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/cert/cert_verifier.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "remoting/base/vlog_net_log.h"

namespace remoting {

URLRequestContextGetter::URLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : network_task_runner_(network_task_runner),
      proxy_config_service_(
          net::ProxyConfigService::CreateSystemProxyConfigService(
              network_task_runner)) {}

net::URLRequestContext* URLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_.get()) {
    CreateVlogNetLogObserver();
    net::URLRequestContextBuilder builder;
    builder.DisableHttpCache();

    if (proxy_config_service_) {
      builder.set_proxy_config_service(std::move(proxy_config_service_));
    }
    cert_net_fetcher_ = base::MakeRefCounted<net::CertNetFetcherURLRequest>();
    auto cert_verifier = net::CertVerifier::CreateDefault(cert_net_fetcher_);
    builder.SetCertVerifier(std::move(cert_verifier));
    url_request_context_ = builder.Build();
    cert_net_fetcher_->SetURLRequestContext(url_request_context_.get());
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
URLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

URLRequestContextGetter::~URLRequestContextGetter() {
  if (cert_net_fetcher_) {
    cert_net_fetcher_->Shutdown();
  }
}

}  // namespace remoting
