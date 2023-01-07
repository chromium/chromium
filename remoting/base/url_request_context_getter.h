// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_URL_REQUEST_CONTEXT_GETTER_H_
#define REMOTING_BASE_URL_REQUEST_CONTEXT_GETTER_H_

#include <memory>

#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class ProxyConfigService;
}  // namespace net

namespace remoting {

class URLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit URLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

  URLRequestContextGetter(const URLRequestContextGetter&) = delete;
  URLRequestContextGetter& operator=(const URLRequestContextGetter&) = delete;

  // Overridden from net::URLRequestContextGetter:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~URLRequestContextGetter() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  scoped_refptr<net::CertNetFetcherURLRequest> cert_net_fetcher_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_URL_REQUEST_CONTEXT_GETTER_H_
