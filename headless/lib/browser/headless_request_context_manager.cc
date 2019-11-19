// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_request_context_manager.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/cors_exempt_headers.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/resource_context.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_auth_preferences.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/url_request_context_builder_mojo.h"

namespace headless {

namespace {

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
static char kProductName[] = "HeadlessChrome";
#endif

net::NetworkTrafficAnnotationTag GetProxyConfigTrafficAnnotationTag() {
  static net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("proxy_config_headless", R"(
    semantics {
      sender: "Proxy Config"
      description:
        "Creates a proxy based on configuration received from headless "
        "command prompt."
      trigger:
        "User starts headless with proxy config."
      data:
        "Proxy configurations."
      destination: OTHER
      destination_other:
        "The proxy server specified in the configuration."
    }
    policy {
      cookies_allowed: NO
      setting:
        "This config is only used for headless mode and provided by user."
      policy_exception_justification:
        "This config is only used for headless mode and provided by user."
    })");
  return traffic_annotation;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
::network::mojom::CryptConfigPtr BuildCryptConfigOnce(
    const base::FilePath& user_data_path) {
  static bool done_once = false;
  if (done_once)
    return nullptr;
  done_once = true;
  ::network::mojom::CryptConfigPtr config =
      ::network::mojom::CryptConfig::New();
  config->store = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kPasswordStore);
  config->product_name = kProductName;
  config->should_use_preference = false;
  config->user_data_path = user_data_path;
  return config;
}
#endif

}  // namespace

// Tracks the ProxyConfig to use, and passes any updates to a NetworkContext's
// ProxyConfigClient.
class HeadlessProxyConfigMonitor
    : public net::ProxyConfigService::Observer,
      public ::network::mojom::ProxyConfigPollerClient {
 public:
  static void DeleteSoon(std::unique_ptr<HeadlessProxyConfigMonitor> instance) {
    instance->task_runner_->DeleteSoon(FROM_HERE, instance.release());
  }

  explicit HeadlessProxyConfigMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // We must create the proxy config service on the UI loop on Linux because
    // it must synchronously run on the glib message loop.
    proxy_config_service_ =
        net::ProxyResolutionService::CreateSystemProxyConfigService(
            task_runner_);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&net::ProxyConfigService::AddObserver,
                                  base::Unretained(proxy_config_service_.get()),
                                  base::Unretained(this)));
  }

  ~HeadlessProxyConfigMonitor() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    proxy_config_service_->RemoveObserver(this);
  }

  // Populates proxy-related fields of |network_context_params|. Updated
  // ProxyConfigs will be sent to a NetworkContext created with those params
  // whenever the configuration changes. Can be called more than once to inform
  // multiple NetworkContexts of proxy changes.
  void AddToNetworkContextParams(
      ::network::mojom::NetworkContextParams* network_context_params) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!proxy_config_client_);
    network_context_params->proxy_config_client_receiver =
        proxy_config_client_.BindNewPipeAndPassReceiver();
    poller_receiver_.Bind(network_context_params->proxy_config_poller_client
                              .InitWithNewPipeAndPassReceiver());
    net::ProxyConfigWithAnnotation proxy_config;
    net::ProxyConfigService::ConfigAvailability availability =
        proxy_config_service_->GetLatestProxyConfig(&proxy_config);
    if (availability != net::ProxyConfigService::CONFIG_PENDING)
      network_context_params->initial_proxy_config = proxy_config;
  }

 private:
  // net::ProxyConfigService::Observer implementation:
  void OnProxyConfigChanged(
      const net::ProxyConfigWithAnnotation& config,
      net::ProxyConfigService::ConfigAvailability availability) override {
    if (!proxy_config_client_)
      return;
    switch (availability) {
      case net::ProxyConfigService::CONFIG_VALID:
        proxy_config_client_->OnProxyConfigUpdated(config);
        break;
      case net::ProxyConfigService::CONFIG_UNSET:
        proxy_config_client_->OnProxyConfigUpdated(
            net::ProxyConfigWithAnnotation::CreateDirect());
        break;
      case net::ProxyConfigService::CONFIG_PENDING:
        NOTREACHED();
        break;
    }
  }

  // network::mojom::ProxyConfigPollerClient implementation:
  void OnLazyProxyConfigPoll() override { proxy_config_service_->OnLazyPoll(); }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  mojo::Receiver<::network::mojom::ProxyConfigPollerClient> poller_receiver_{
      this};
  mojo::Remote<::network::mojom::ProxyConfigClient> proxy_config_client_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessProxyConfigMonitor);
};

// static
std::unique_ptr<HeadlessRequestContextManager>
HeadlessRequestContextManager::CreateSystemContext(
    const HeadlessBrowserContextOptions* options) {
  auto manager = std::make_unique<HeadlessRequestContextManager>(
      options, base::FilePath());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  auto auth_params = ::network::mojom::HttpAuthDynamicParams::New();
  auth_params->server_allowlist =
      command_line->GetSwitchValueASCII(switches::kAuthServerWhitelist);
  auto* network_service = content::GetNetworkService();
  network_service->ConfigureHttpAuthPrefs(std::move(auth_params));

  network_service->CreateNetworkContext(
      manager->system_context_.InitWithNewPipeAndPassReceiver(),
      manager->CreateNetworkContextParams(/* is_system = */ true));

  return manager;
}

HeadlessRequestContextManager::HeadlessRequestContextManager(
    const HeadlessBrowserContextOptions* options,
    base::FilePath user_data_path)
    : cookie_encryption_enabled_(
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableCookieEncryption)),
      user_data_path_(std::move(user_data_path)),
      accept_language_(options->accept_language()),
      user_agent_(options->user_agent()),
      proxy_config_(
          options->proxy_config()
              ? std::make_unique<net::ProxyConfig>(*options->proxy_config())
              : nullptr),
      resource_context_(std::make_unique<content::ResourceContext>()) {
  if (!proxy_config_) {
    proxy_config_monitor_ = std::make_unique<HeadlessProxyConfigMonitor>(
        base::ThreadTaskRunnerHandle::Get());
  }
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  auto crypt_config = BuildCryptConfigOnce(user_data_path_);
  if (crypt_config)
    content::GetNetworkService()->SetCryptConfig(std::move(crypt_config));
#endif
}

HeadlessRequestContextManager::~HeadlessRequestContextManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (proxy_config_monitor_)
    HeadlessProxyConfigMonitor::DeleteSoon(std::move(proxy_config_monitor_));
}

mojo::Remote<::network::mojom::NetworkContext>
HeadlessRequestContextManager::CreateNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  mojo::Remote<::network::mojom::NetworkContext> network_context;
  content::GetNetworkService()->CreateNetworkContext(
      network_context.BindNewPipeAndPassReceiver(),
      CreateNetworkContextParams(/* is_system = */ false));
  return network_context;
}

::network::mojom::NetworkContextParamsPtr
HeadlessRequestContextManager::CreateNetworkContextParams(bool is_system) {
  auto context_params = ::network::mojom::NetworkContextParams::New();

  context_params->user_agent = user_agent_;
  context_params->accept_language = accept_language_;
  context_params->primary_network_context = is_system;

  // TODO(https://crbug.com/458508): Allow
  // context_params->http_auth_static_network_context_params->allow_default_credentials
  // to be controllable by a flag.
  context_params->http_auth_static_network_context_params =
      ::network::mojom::HttpAuthStaticNetworkContextParams::New();

  if (!user_data_path_.empty()) {
    context_params->enable_encrypted_cookies = cookie_encryption_enabled_;
    context_params->cookie_path =
        user_data_path_.Append(FILE_PATH_LITERAL("Cookies"));
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDiskCacheDir)) {
    context_params->http_cache_path =
        command_line->GetSwitchValuePath(switches::kDiskCacheDir);
  } else if (!user_data_path_.empty()) {
    context_params->http_cache_path =
        user_data_path_.Append(FILE_PATH_LITERAL("Cache"));
  }
  if (proxy_config_) {
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        *proxy_config_, GetProxyConfigTrafficAnnotationTag());
  } else {
    proxy_config_monitor_->AddToNetworkContextParams(context_params.get());
  }
  content::UpdateCorsExemptHeader(context_params.get());
  return context_params;
}

}  // namespace headless
