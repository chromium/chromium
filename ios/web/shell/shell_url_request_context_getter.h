// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
#define IOS_WEB_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class ProxyConfigService;
class URLRequestContext;
class SystemCookieStore;
}  // namespace net

namespace web {

class BrowserState;

class ShellURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  ShellURLRequestContextGetter(
      const base::FilePath& base_path,
      web::BrowserState* browser_state,
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);

  ShellURLRequestContextGetter(const ShellURLRequestContextGetter&) = delete;
  ShellURLRequestContextGetter& operator=(const ShellURLRequestContextGetter&) =
      delete;

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
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  // SystemCookieStore must be created on UI thread in
  // ShellURLRequestContextGetter's constructor. Later the ownership is passed
  // to net::URLRequestContext on IO thread. `system_cookie_store_` is
  // created in constructor and cleared in GetURLRequestContext() where
  // net::URLRequestContext is created.
  std::unique_ptr<net::SystemCookieStore> system_cookie_store_;
};

}  // namespace web

#endif  // IOS_WEB_SHELL_SHELL_URL_REQUEST_CONTEXT_GETTER_H_
