// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/shell/shell_url_request_context_getter.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#import "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/public/browsing_data/system_cookie_store_util.h"
#import "ios/web/public/web_client.h"
#include "net/base/cache_type.h"
#include "net/base/network_delegate_impl.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_persister.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config_service_ios.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

ShellURLRequestContextGetter::ShellURLRequestContextGetter(
    const base::FilePath& base_path,
    web::BrowserState* browser_state,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : base_path_(base_path),
      network_task_runner_(network_task_runner),
      proxy_config_service_(
          new net::ProxyConfigServiceIOS(NO_TRAFFIC_ANNOTATION_YET)),
      system_cookie_store_(web::CreateSystemCookieStore(browser_state)) {}

ShellURLRequestContextGetter::~ShellURLRequestContextGetter() {}

net::URLRequestContext* ShellURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!url_request_context_) {
    net::URLRequestContextBuilder builder;
    builder.set_network_delegate(std::make_unique<net::NetworkDelegateImpl>());
    builder.SetCookieStore(std::make_unique<net::CookieStoreIOS>(
        std::move(system_cookie_store_), net::NetLog::Get()));
    std::string user_agent =
        web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE);
    builder.set_http_user_agent_settings(
        std::make_unique<net::StaticHttpUserAgentSettings>("en-us,en",
                                                           user_agent));
    builder.set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
            std::move(proxy_config_service_), net::NetLog::Get(),
            /*quick_check_enabled=*/true));
    builder.set_host_resolver(
        net::HostResolver::CreateStandaloneResolver(net::NetLog::Get()));
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
    cache_params.path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
    builder.EnableHttpCache(cache_params);
    url_request_context_ = builder.Build();

    base::FilePath transport_security_state_file_path =
        base_path_.Append(FILE_PATH_LITERAL("TransportSecurity"));
    transport_security_persister_ =
        std::make_unique<net::TransportSecurityPersister>(
            url_request_context_->transport_security_state(),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
            transport_security_state_file_path);
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
ShellURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

}  // namespace web
