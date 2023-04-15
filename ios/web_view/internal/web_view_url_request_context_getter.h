// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_URL_REQUEST_CONTEXT_GETTER_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class NetLog;
class ProxyConfigService;
class URLRequestContext;
class SystemCookieStore;
}  // namespace net

namespace web {
class BrowserState;
}

namespace ios_web_view {

// WebView implementation of URLRequestContextGetter.
class WebViewURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  WebViewURLRequestContextGetter(
      const base::FilePath& base_path,
      web::BrowserState* browser_state,
      net::NetLog* net_log,
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);

  WebViewURLRequestContextGetter(const WebViewURLRequestContextGetter&) =
      delete;
  WebViewURLRequestContextGetter& operator=(
      const WebViewURLRequestContextGetter&) = delete;

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  // Discard reference to URLRequestContext and inform observers of shutdown.
  // Must be called before destruction. May only be called on IO thread.
  void ShutDown();

 protected:
  ~WebViewURLRequestContextGetter() override;

 private:
  // Member list should be maintained to ensure proper destruction order.
  base::FilePath base_path_;
  net::NetLog* net_log_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  // SystemCookieStore must be created on UI thread in
  // WebViewURLRequestContextGetter's constructor. Later the ownership is passed
  // to net::URLRequestContext on IO thread. |system_cookie_store_| is
  // created in constructor and cleared in GetURLRequestContext() where
  // net::URLRequestContext is created.
  std::unique_ptr<net::SystemCookieStore> system_cookie_store_;
  // Protocol handler for web ui.
  std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler> protocol_handler_;

  // Used to ensure GetURLRequestContext() returns nullptr during shut down.
  bool is_shutting_down_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_URL_REQUEST_CONTEXT_GETTER_H_
