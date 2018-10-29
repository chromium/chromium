// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/mojom/network_context.mojom.h"

#include <string>

namespace content {
class ResourceContext;
}

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace headless {

class HeadlessBrowserContextOptions;
class HeadlessProxyConfigMonitor;
class HeadlessResourceContext;
class HeadlessURLRequestContextGetter;

class HeadlessRequestContextManager {
 public:
  static std::unique_ptr<HeadlessRequestContextManager> CreateSystemContext(
      const HeadlessBrowserContextOptions* options);

  explicit HeadlessRequestContextManager(
      const HeadlessBrowserContextOptions* options,
      base::FilePath user_data_path);
  ~HeadlessRequestContextManager();

  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);

  ::network::mojom::NetworkContextPtr CreateNetworkContext(
      bool in_memory,
      const base::FilePath& relative_partition_path);

  content::ResourceContext* GetResourceContext();
  net::URLRequestContextGetter* url_request_context_getter();

 private:
  void Initialize();
  void InitializeOnIO();
  void MaybeSetUpOSCrypt();

  ::network::mojom::NetworkContextParamsPtr CreateNetworkContextParams();

  const bool network_service_enabled_;
  const bool cookie_encryption_enabled_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::FilePath user_data_path_;
  std::string accept_language_;
  std::string user_agent_;
  std::unique_ptr<net::ProxyConfig> proxy_config_;
  std::unique_ptr<HeadlessProxyConfigMonitor> proxy_config_monitor_;
  bool is_system_context_;

  ::network::mojom::NetworkContextPtr network_context_;
  ::network::mojom::NetworkContextRequest network_context_request_;

  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;

  std::unique_ptr<::network::mojom::NetworkContext> network_context_owner_;
  scoped_refptr<HeadlessURLRequestContextGetter> url_request_context_getter_;
  std::unique_ptr<HeadlessResourceContext> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessRequestContextManager);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_
