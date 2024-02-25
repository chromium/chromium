// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace headless {

class HeadlessBrowserContextOptions;
class HeadlessProxyConfigMonitor;

class HeadlessRequestContextManager {
 public:
  static std::unique_ptr<HeadlessRequestContextManager> CreateSystemContext(
      const HeadlessBrowserContextOptions* options);

  HeadlessRequestContextManager(const HeadlessBrowserContextOptions* options,
                                base::FilePath user_data_path);

  HeadlessRequestContextManager(const HeadlessRequestContextManager&) = delete;
  HeadlessRequestContextManager& operator=(
      const HeadlessRequestContextManager&) = delete;

  ~HeadlessRequestContextManager();

  void ConfigureNetworkContextParams(
      bool in_memory,
      const base::FilePath& relative_partition_path,
      ::network::mojom::NetworkContextParams* network_context_params,
      ::cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

 private:
  void ConfigureNetworkContextParamsInternal(
      ::network::mojom::NetworkContextParams* network_context_params,
      ::cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

  const bool cookie_encryption_enabled_;

  base::FilePath user_data_path_;
  base::FilePath disk_cache_dir_;
  std::string accept_language_;
  std::string user_agent_;
  std::unique_ptr<net::ProxyConfig> proxy_config_;
  std::unique_ptr<HeadlessProxyConfigMonitor> proxy_config_monitor_;

  mojo::PendingRemote<::network::mojom::NetworkContext> system_context_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_REQUEST_CONTEXT_MANAGER_H_
