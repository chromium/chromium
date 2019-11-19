// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

#include <string>

namespace content {
class ResourceContext;
}

namespace headless {

class HeadlessBrowserContextOptions;
class HeadlessProxyConfigMonitor;

class HeadlessRequestContextManager {
 public:
  static std::unique_ptr<HeadlessRequestContextManager> CreateSystemContext(
      const HeadlessBrowserContextOptions* options);

  HeadlessRequestContextManager(const HeadlessBrowserContextOptions* options,
                                base::FilePath user_data_path);
  ~HeadlessRequestContextManager();

  mojo::Remote<::network::mojom::NetworkContext> CreateNetworkContext(
      bool in_memory,
      const base::FilePath& relative_partition_path);

  content::ResourceContext* GetResourceContext() {
    return resource_context_.get();
  }

 private:
  ::network::mojom::NetworkContextParamsPtr CreateNetworkContextParams(
      bool is_system);

  const bool cookie_encryption_enabled_;

  base::FilePath user_data_path_;
  std::string accept_language_;
  std::string user_agent_;
  std::unique_ptr<net::ProxyConfig> proxy_config_;
  std::unique_ptr<HeadlessProxyConfigMonitor> proxy_config_monitor_;

  mojo::PendingRemote<::network::mojom::NetworkContext> system_context_;
  std::unique_ptr<content::ResourceContext> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessRequestContextManager);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_
