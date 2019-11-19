// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
#define IOS_WEB_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class NetworkDelegate;
class NetLog;
class ProxyConfigService;
class TransportSecurityPersister;
class URLRequestContext;
class URLRequestContextStorage;
class SystemCookieStore;
}

namespace web {

class BrowserState;

class ShellURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  ShellURLRequestContextGetter(
      const base::FilePath& base_path,
      web::BrowserState* browser_state,
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~ShellURLRequestContextGetter() override;

 private:
  base::FilePath base_path_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::NetworkDelegate> network_delegate_;
  std::unique_ptr<net::URLRequestContextStorage> storage_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<net::NetLog> net_log_;
  std::unique_ptr<net::TransportSecurityPersister>
      transport_security_persister_;
  // SystemCookieStore must be created on UI thread in
  // ShellURLRequestContextGetter's constructor. Later the ownership is passed
  // to net::URLRequestContextStorage on IO thread. |system_cookie_store_| is
  // created in constructor and cleared in GetURLRequestContext() where
  // net::URLRequestContextStorage is lazily created.
  std::unique_ptr<net::SystemCookieStore> system_cookie_store_;

  DISALLOW_COPY_AND_ASSIGN(ShellURLRequestContextGetter);
};

}  // namespace web

#endif  // IOS_WEB_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
