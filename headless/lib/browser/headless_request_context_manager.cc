// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_request_context_manager.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/public/switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/features.h"
#include "net/http/http_auth_preferences.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/url_request_context_builder_mojo.h"

namespace headless {

namespace {

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
        net::ProxyConfigService::CreateSystemProxyConfigService(task_runner_);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&net::ProxyConfigService::AddObserver,
                                  base::Unretained(proxy_config_service_.get()),
                                  base::Unretained(this)));
  }

  HeadlessProxyConfigMonitor(const HeadlessProxyConfigMonitor&) = delete;
  HeadlessProxyConfigMonitor& operator=(const HeadlessProxyConfigMonitor&) =
      delete;

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
    if (proxy_config_client_) {
      // This may be called in the course of re-connecting to a new instance
      // of network service following a restart, so the config client / poller
      // interfaces may have been previously bound.
      proxy_config_client_.reset();
      poller_receiver_.reset();
    }
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
    }
  }

  // network::mojom::ProxyConfigPollerClient implementation:
  void OnLazyProxyConfigPoll() override { proxy_config_service_->OnLazyPoll(); }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  mojo::Receiver<::network::mojom::ProxyConfigPollerClient> poller_receiver_{
      this};
  mojo::Remote<::network::mojom::ProxyConfigClient> proxy_config_client_;
};

// static
std::unique_ptr<HeadlessRequestContextManager>
HeadlessRequestContextManager::CreateSystemContext(
    const HeadlessBrowserContextOptions* options) {
  auto manager = std::make_unique<HeadlessRequestContextManager>(
      options, base::FilePath());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  auto auth_params = ::network::mojom::HttpAuthDynamicParams::New();

  if (command_line->HasSwitch(switches::kAuthServerAllowlist)) {
    auth_params->server_allowlist =
        command_line->GetSwitchValueASCII(switches::kAuthServerAllowlist);
  }

  auto* network_service = content::GetNetworkService();
  network_service->ConfigureHttpAuthPrefs(std::move(auth_params));

  ::network::mojom::NetworkContextParamsPtr network_context_params =
      ::network::mojom::NetworkContextParams::New();
  ::cert_verifier::mojom::CertVerifierCreationParamsPtr
      cert_verifier_creation_params =
          ::cert_verifier::mojom::CertVerifierCreationParams::New();
  manager->ConfigureNetworkContextParamsInternal(
      network_context_params.get(), cert_verifier_creation_params.get());
  network_context_params->cert_verifier_params =
      content::GetCertVerifierParams(std::move(cert_verifier_creation_params));
  content::CreateNetworkContextInNetworkService(
      manager->system_context_.InitWithNewPipeAndPassReceiver(),
      std::move(network_context_params));

  return manager;
}

HeadlessRequestContextManager::HeadlessRequestContextManager(
    const HeadlessBrowserContextOptions* options,
    base::FilePath user_data_path)
    :
// On Windows, Cookie encryption requires access to local_state prefs.
#if BUILDFLAG(IS_WIN) && !defined(HEADLESS_USE_PREFS)
      cookie_encryption_enabled_(false),
#else
      cookie_encryption_enabled_(
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableCookieEncryption)),
#endif
      user_data_path_(std::move(user_data_path)),
      disk_cache_dir_(options->disk_cache_dir()),
      accept_language_(options->accept_language()),
      user_agent_(options->user_agent()),
      proxy_config_(
          options->proxy_config()
              ? std::make_unique<net::ProxyConfig>(*options->proxy_config())
              : nullptr) {
  if (!proxy_config_) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kNoSystemProxyConfigService)) {
      proxy_config_ = std::make_unique<net::ProxyConfig>();
    } else {
      proxy_config_monitor_ = std::make_unique<HeadlessProxyConfigMonitor>(
          base::SingleThreadTaskRunner::GetCurrentDefault());
    }
  }
}

HeadlessRequestContextManager::~HeadlessRequestContextManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (proxy_config_monitor_)
    HeadlessProxyConfigMonitor::DeleteSoon(std::move(proxy_config_monitor_));
}

void HeadlessRequestContextManager::ConfigureNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    ::network::mojom::NetworkContextParams* network_context_params,
    ::cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  ConfigureNetworkContextParamsInternal(network_context_params,
                                        cert_verifier_creation_params);
}

void HeadlessRequestContextManager::ConfigureNetworkContextParamsInternal(
    ::network::mojom::NetworkContextParams* context_params,
    ::cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  context_params->user_agent = user_agent_;
  context_params->accept_language = accept_language_;
  context_params->enable_zstd =
      base::FeatureList::IsEnabled(net::features::kZstdContentEncoding);

  // TODO(crbug.com/40405715): Allow
  // context_params->http_auth_static_network_context_params->allow_default_credentials
  // to be controllable by a flag.
  context_params->http_auth_static_network_context_params =
      ::network::mojom::HttpAuthStaticNetworkContextParams::New();

  if (!user_data_path_.empty()) {
    context_params->enable_encrypted_cookies = cookie_encryption_enabled_;
    context_params->file_paths =
        ::network::mojom::NetworkContextFilePaths::New();
    context_params->file_paths->data_directory =
        user_data_path_.Append(FILE_PATH_LITERAL("Network"));
    context_params->file_paths->unsandboxed_data_path = user_data_path_;
    context_params->file_paths->cookie_database_name =
        base::FilePath(FILE_PATH_LITERAL("Cookies"));
#if BUILDFLAG(IS_WIN)
    // For the network sandbox to operate, the network data must be in the
    // 'Network' directory and not the `unsandboxed_data_path`.
    //
    // On Windows, the majority of data dir is already residing in the 'Network'
    // data dir. This is because there are three possible cases:
    // 1. A data dir from headful is being used, and headful has migrated any
    // data since M98 (Feb 2022). (data is in 'Network')
    // 2. Headless has been using the 'Network' data dir since M96, since
    // although headless never opted into migration, any new data was always in
    // the 'Network' dir since ba2eb47b. (data is in 'Network')
    // 3. Headless is using a data dir from before M96, and since migration was
    // never enabled, it has been continuing to use this directory up until now.
    // (data is in `unsandboxed_data_path` and sandbox will not function).
    //
    // The majority of users are in 1, or 2. For the small number of users in 3,
    // setting `trigger_migration` will migrate their data dirs to 'Network' but
    // this data will still interop between headless and headful as long as they
    // are running M96 or later that understands both directories, the only
    // noticeable difference will be that the files will move, as they have
    // already been doing in headful.
    context_params->file_paths->trigger_migration = true;
#else
    // On non-Windows platforms, trigger migration is not set, so there is no
    // point (but equally no harm) in doing a migration since headful does not
    // perform the migration. See
    // ProfileNetworkContextService::ConfigureNetworkContextParamsInternal in
    // src/chrome.
    context_params->file_paths->trigger_migration = false;
#endif  // BUILDFLAG(IS_WIN)
  }

  if (!disk_cache_dir_.empty()) {
    if (!context_params->file_paths) {
      context_params->file_paths =
          ::network::mojom::NetworkContextFilePaths::New();
    }
    context_params->file_paths->http_cache_directory = disk_cache_dir_;
  } else if (!user_data_path_.empty()) {
    context_params->file_paths->http_cache_directory =
        user_data_path_.Append(FILE_PATH_LITERAL("Cache"));
  }
  if (proxy_config_) {
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        *proxy_config_, GetProxyConfigTrafficAnnotationTag());
  } else {
    proxy_config_monitor_->AddToNetworkContextParams(context_params);
  }
}

}  // namespace headless
