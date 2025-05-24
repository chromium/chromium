// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_url_request_context_getter.h"

#import <utility>

#import "base/base_paths.h"
#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/memory/ref_counted.h"
#import "base/path_service.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/thread_pool.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/web/public/browsing_data/system_cookie_store_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/webui/url_data_manager_ios_backend.h"
#import "net/http/http_network_session.h"
#import "net/http/transport_security_persister.h"
#import "net/http/transport_security_state.h"
#import "net/log/net_log.h"
#import "net/proxy_resolution/proxy_config_service_ios.h"
#import "net/ssl/ssl_config_service_defaults.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "net/url_request/url_request_context.h"
#import "net/url_request/url_request_context_builder.h"

namespace ios_web_view {

WebViewURLRequestContextGetter::WebViewURLRequestContextGetter(
    const base::FilePath& base_path,
    web::BrowserState* browser_state,
    net::NetLog* net_log,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : base_path_(base_path),
      net_log_(net_log),
      network_task_runner_(network_task_runner),
      proxy_config_service_(
          new net::ProxyConfigServiceIOS(NO_TRAFFIC_ANNOTATION_YET)),
      system_cookie_store_(web::CreateSystemCookieStore(browser_state)),
      protocol_handler_(
          web::URLDataManagerIOSBackend::CreateProtocolHandler(browser_state)),
      is_shutting_down_(false) {}

WebViewURLRequestContextGetter::~WebViewURLRequestContextGetter() = default;

net::URLRequestContext* WebViewURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (is_shutting_down_) {
    return nullptr;
  }

  if (!url_request_context_) {
    web::WebClient* web_client = web::GetWebClient();
    DCHECK(web_client);

    net::URLRequestContextBuilder builder;
    builder.set_net_log(net_log_);
    builder.SetCookieStore(std::make_unique<net::CookieStoreIOS>(
        std::move(system_cookie_store_), net_log_));

    builder.set_accept_language("en-us,en");
    builder.set_user_agent(
        web_client->GetUserAgent(web::UserAgentType::MOBILE));
    builder.set_proxy_config_service(std::move(proxy_config_service_));
    builder.set_transport_security_persister_file_path(
        base_path_.Append(FILE_PATH_LITERAL("TransportSecurity")));

    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::DISK;
    cache_params.path =
        base_path_.Append(FILE_PATH_LITERAL("ChromeWebViewCache"));
    builder.EnableHttpCache(cache_params);

    builder.SetProtocolHandler(kChromeUIScheme, std::move(protocol_handler_));
    url_request_context_ = builder.Build();
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebViewURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

void WebViewURLRequestContextGetter::ShutDown() {
  is_shutting_down_ = true;
  net::URLRequestContextGetter::NotifyContextShuttingDown();
}

}  // namespace ios_web_view
