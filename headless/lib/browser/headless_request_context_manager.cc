// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_request_context_manager.h"

#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "components/os_crypt/os_crypt.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/devtools_network_transaction_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/resource_context.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "net/base/network_delegate_impl.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/url_request_context_builder_mojo.h"

namespace headless {

namespace {

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
static char kProductName[] = "HeadlessChrome";
#endif

class DelegateImpl : public net::NetworkDelegateImpl {
 public:
  DelegateImpl() = default;
  ~DelegateImpl() override = default;

 private:
  // net::NetworkDelegateImpl implementation.
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override {
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(DelegateImpl);
};

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

// Contains net::URLRequestContextGetter required for resource loading.
// Must be destructed on the IO thread as per content::ResourceContext
// requirements.
class HeadlessResourceContext : public content::ResourceContext {
 public:
  HeadlessResourceContext() = default;
  ~HeadlessResourceContext() override {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  }

  // ResourceContext implementation:
  net::URLRequestContext* GetRequestContext() override {
    CHECK(url_request_context_getter_);
    return url_request_context_getter_->GetURLRequestContext();
  }

  // Configure the URL request context getter to be used for resource fetching.
  // Must be called before any of the other methods of this class are used.
  void set_url_request_context_getter(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    url_request_context_getter_ = std::move(url_request_context_getter);
  }

 private:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessResourceContext);
};

class HeadlessURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit HeadlessURLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : context_(nullptr), task_runner_(task_runner) {}

  net::URLRequestContext* GetURLRequestContext() override { return context_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return task_runner_;
  }

  void SetURLRequestContext(net::URLRequestContext* context) {
    DCHECK(!context_ && !context_owner_);
    DCHECK(!base::FeatureList::IsEnabled(::network::features::kNetworkService));

    context_ = context;
  }

  void SetURLRequestContext(std::unique_ptr<net::URLRequestContext> context) {
    DCHECK(!context_ && !context_owner_);
    DCHECK(base::FeatureList::IsEnabled(::network::features::kNetworkService));

    context_owner_ = std::move(context);
    context_ = context_owner_.get();
  }

  void Shutdown() {
    context_ = nullptr;
    NotifyContextShuttingDown();
    if (context_owner_)
      task_runner_->DeleteSoon(FROM_HERE, context_owner_.release());
  }

 private:
  ~HeadlessURLRequestContextGetter() override { DCHECK(!context_); }

  net::URLRequestContext* context_;
  std::unique_ptr<net::URLRequestContext> context_owner_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

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
      : task_runner_(task_runner), poller_binding_(this) {
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
    network_context_params->proxy_config_client_request =
        mojo::MakeRequest(&proxy_config_client_);
    poller_binding_.Bind(
        mojo::MakeRequest(&network_context_params->proxy_config_poller_client));
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
  mojo::Binding<::network::mojom::ProxyConfigPollerClient> poller_binding_;
  ::network::mojom::ProxyConfigClientPtr proxy_config_client_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessProxyConfigMonitor);
};

// static
std::unique_ptr<HeadlessRequestContextManager>
HeadlessRequestContextManager::CreateSystemContext(
    const HeadlessBrowserContextOptions* options) {
  auto manager = std::make_unique<HeadlessRequestContextManager>(
      options, base::FilePath());
  manager->is_system_context_ = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  auto auth_params = ::network::mojom::HttpAuthDynamicParams::New();
  auth_params->server_whitelist =
      command_line->GetSwitchValueASCII(switches::kAuthServerWhitelist);
  auto* network_service = content::GetNetworkService();
  network_service->ConfigureHttpAuthPrefs(std::move(auth_params));

  if (!manager->network_service_enabled_) {
    manager->Initialize();
    return manager;
  }
  network_service->CreateNetworkContext(MakeRequest(&manager->network_context_),
                                        manager->CreateNetworkContextParams());

  return manager;
}

HeadlessRequestContextManager::HeadlessRequestContextManager(
    const HeadlessBrowserContextOptions* options,
    base::FilePath user_data_path)
    : network_service_enabled_(
          base::FeatureList::IsEnabled(::network::features::kNetworkService)),
      cookie_encryption_enabled_(
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableCookieEncryption)),
      io_task_runner_(base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO})),
      user_data_path_(std::move(user_data_path)),
      accept_language_(options->accept_language()),
      user_agent_(options->user_agent()),
      proxy_config_(
          options->proxy_config()
              ? std::make_unique<net::ProxyConfig>(*options->proxy_config())
              : nullptr),
      is_system_context_(false),
      resource_context_(std::make_unique<HeadlessResourceContext>()) {
  if (!proxy_config_) {
    auto proxy_monitor_task_runner = network_service_enabled_
                                         ? base::ThreadTaskRunnerHandle::Get()
                                         : io_task_runner_;
    proxy_config_monitor_ =
        std::make_unique<HeadlessProxyConfigMonitor>(proxy_monitor_task_runner);
  }
  MaybeSetUpOSCrypt();
}

HeadlessRequestContextManager::~HeadlessRequestContextManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (url_request_context_getter_)
    url_request_context_getter_->Shutdown();
  if (proxy_config_monitor_)
    HeadlessProxyConfigMonitor::DeleteSoon(std::move(proxy_config_monitor_));
}

net::URLRequestContextGetter*
HeadlessRequestContextManager::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  request_interceptors_ = std::move(request_interceptors);
  protocol_handlers_.swap(*protocol_handlers);
  Initialize();
  return url_request_context_getter_.get();
}

::network::mojom::NetworkContextPtr
HeadlessRequestContextManager::CreateNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  if (!network_service_enabled_) {
    if (!network_context_) {
      DCHECK(!network_context_request_);
      network_context_request_ = mojo::MakeRequest(&network_context_);
    }
    return std::move(network_context_);
  }
  content::GetNetworkService()->CreateNetworkContext(
      MakeRequest(&network_context_), CreateNetworkContextParams());
  return std::move(network_context_);
}

content::ResourceContext* HeadlessRequestContextManager::GetResourceContext() {
  return resource_context_.get();
}

net::URLRequestContextGetter*
HeadlessRequestContextManager::url_request_context_getter() {
  return url_request_context_getter_.get();
}

void HeadlessRequestContextManager::Initialize() {
  url_request_context_getter_ =
      base::MakeRefCounted<HeadlessURLRequestContextGetter>(io_task_runner_);
  resource_context_->set_url_request_context_getter(
      url_request_context_getter_);
  if (!network_context_) {
    DCHECK(!network_context_request_);
    network_context_request_ = mojo::MakeRequest(&network_context_);
  }
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HeadlessRequestContextManager::InitializeOnIO,
                                base::Unretained(this)));
}

void HeadlessRequestContextManager::InitializeOnIO() {
  if (!network_service_enabled_) {
    DCHECK(network_context_request_);

    auto builder = std::make_unique<::network::URLRequestContextBuilderMojo>();
    builder->set_network_delegate(std::make_unique<DelegateImpl>());
    builder->SetCreateHttpTransactionFactoryCallback(
        base::BindOnce(&content::CreateDevToolsNetworkTransactionFactory));
    builder->SetInterceptors(std::move(request_interceptors_));
    for (auto& protocol_handler : protocol_handlers_) {
      builder->SetProtocolHandler(protocol_handler.first,
                                  std::move(protocol_handler.second));
    }
    protocol_handlers_.clear();

    net::URLRequestContext* url_request_context = nullptr;
    network_context_owner_ =
        content::GetNetworkServiceImpl()->CreateNetworkContextWithBuilder(
            std::move(network_context_request_), CreateNetworkContextParams(),
            std::move(builder), &url_request_context);

    url_request_context_getter_->SetURLRequestContext(url_request_context);
    return;
  }

  net::URLRequestContextBuilder builder;
  builder.set_proxy_resolution_service(
      net::ProxyResolutionService::CreateDirect());
  url_request_context_getter_->SetURLRequestContext(builder.Build());
}

void HeadlessRequestContextManager::MaybeSetUpOSCrypt() {
  static bool initialized = false;
  if (initialized || !cookie_encryption_enabled_)
    return;
  if (user_data_path_.empty())
    return;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  ::network::mojom::CryptConfigPtr config =
      ::network::mojom::CryptConfig::New();
  config->store = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kPasswordStore);
  config->product_name = kProductName;
  config->should_use_preference = false;
  config->user_data_path = user_data_path_;
  content::GetNetworkService()->SetCryptConfig(std::move(config));
#endif
  initialized = true;
}

::network::mojom::NetworkContextParamsPtr
HeadlessRequestContextManager::CreateNetworkContextParams() {
  auto context_params = ::network::mojom::NetworkContextParams::New();

  context_params->user_agent = user_agent_;
  context_params->accept_language = accept_language_;
  // TODO(skyostil): Make these configurable.
  context_params->enable_data_url_support = true;
  context_params->enable_file_url_support = true;
  context_params->primary_network_context = is_system_context_;

  if (!user_data_path_.empty()) {
    context_params->enable_encrypted_cookies = cookie_encryption_enabled_;
    context_params->cookie_path =
        user_data_path_.Append(FILE_PATH_LITERAL("Cookies"));
    context_params->channel_id_path =
        user_data_path_.Append(FILE_PATH_LITERAL("Origin Bound Certs"));
  }
  if (proxy_config_) {
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        *proxy_config_, GetProxyConfigTrafficAnnotationTag());
  } else {
    proxy_config_monitor_->AddToNetworkContextParams(context_params.get());
  }
  return context_params;
}

}  // namespace headless
