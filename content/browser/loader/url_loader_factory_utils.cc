// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_factory_utils.h"

#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace url_loader_factory {

namespace {

Interceptor& GetMutableInterceptor() {
  static base::NoDestructor<Interceptor> s_callback;
  return *s_callback;
}

bool s_has_interceptor = false;

}  // namespace

const Interceptor& GetTestingInterceptor() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return GetMutableInterceptor();
}

void SetInterceptorForTesting(const Interceptor& interceptor) {
  CHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
        BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(interceptor.is_null() || GetMutableInterceptor().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetMutableInterceptor() = interceptor;
}

bool HasInterceptorOnIOThreadForTesting() {
  CHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
        BrowserThread::CurrentlyOn(BrowserThread::IO));
  return s_has_interceptor;
}

void SetHasInterceptorOnIOThreadForTesting(bool has_interceptor) {
  CHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
        BrowserThread::CurrentlyOn(BrowserThread::IO));
  s_has_interceptor = has_interceptor;
}

TerminalParams::TerminalParams(
    network::mojom::NetworkContext* network_context,
    network::mojom::URLLoaderFactoryParamsPtr factory_params,
    HeaderClientOption header_client_option,
    FactoryOverrideOption factory_override_option,
    DisableSecureDnsOption disable_secure_dns_option,
    StoragePartitionImpl* storage_partition,
    std::optional<URLLoaderFactoryTypes> url_loader_factory,
    int process_id)
    : network_context_(network_context),
      factory_params_(std::move(factory_params)),
      header_client_option_(header_client_option),
      factory_override_option_(factory_override_option),
      disable_secure_dns_option_(disable_secure_dns_option),
      storage_partition_(storage_partition),
      url_loader_factory_(std::move(url_loader_factory)),
      process_id_(process_id) {}

TerminalParams::~TerminalParams() = default;
TerminalParams::TerminalParams(TerminalParams&&) = default;
TerminalParams& TerminalParams::operator=(TerminalParams&&) = default;

// static
TerminalParams TerminalParams::ForNetworkContext(
    network::mojom::NetworkContext* network_context,
    network::mojom::URLLoaderFactoryParamsPtr factory_params,
    HeaderClientOption header_client_option,
    FactoryOverrideOption factory_override_option,
    DisableSecureDnsOption disable_secure_dns_option) {
  CHECK(network_context);
  CHECK(factory_params);
  int process_id = factory_params->process_id;
  return TerminalParams(network_context, std::move(factory_params),
                        header_client_option, factory_override_option,
                        disable_secure_dns_option,
                        /*storage_partition=*/nullptr,
                        /*url_loader_factory=*/std::nullopt, process_id);
}

// static
TerminalParams TerminalParams::ForBrowserProcess(
    StoragePartitionImpl* storage_partition,
    HeaderClientOption header_client_option) {
  CHECK(storage_partition);
  return TerminalParams(storage_partition->GetNetworkContext(),
                        storage_partition->CreateURLLoaderFactoryParams(),
                        header_client_option, FactoryOverrideOption::kDisallow,
                        DisableSecureDnsOption::kDisallow, storage_partition,
                        /*url_loader_factory=*/std::nullopt,
                        network::mojom::kBrowserProcessId);
}

// static
TerminalParams TerminalParams::ForNonNetwork(
    URLLoaderFactoryTypes url_loader_factory,
    int process_id) {
  return TerminalParams(
      /*network_context=*/nullptr, /*factory_params=*/nullptr,
      HeaderClientOption::kDisallow, FactoryOverrideOption::kDisallow,
      DisableSecureDnsOption::kDisallow,
      /*storage_partition=*/nullptr, std::move(url_loader_factory), process_id);
}

network::mojom::NetworkContext* TerminalParams::network_context() const {
  return network_context_.get();
}
HeaderClientOption TerminalParams::header_client_option() const {
  return header_client_option_;
}
FactoryOverrideOption TerminalParams::factory_override_option() const {
  return factory_override_option_;
}
DisableSecureDnsOption TerminalParams::disable_secure_dns_option() const {
  return disable_secure_dns_option_;
}
StoragePartitionImpl* TerminalParams::storage_partition() const {
  return storage_partition_.get();
}
int TerminalParams::process_id() const {
  return process_id_;
}
network::mojom::URLLoaderFactoryParamsPtr TerminalParams::TakeFactoryParams() {
  return std::move(factory_params_);
}
std::optional<TerminalParams::URLLoaderFactoryTypes>
TerminalParams::TakeURLLoaderFactory() {
  return std::move(url_loader_factory_);
}

ContentClientParams::ContentClientParams(
    BrowserContext* browser_context,
    RenderFrameHost* frame,
    int render_process_id,
    const url::Origin& request_initiator,
    const net::IsolationInfo& isolation_info,
    ukm::SourceIdObj ukm_source_id,
    bool* bypass_redirect_checks,
    std::optional<int64_t> navigation_id,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
    : browser_context_(browser_context),
      frame_(frame),
      render_process_id_(render_process_id),
      request_initiator_(request_initiator),
      isolation_info_(isolation_info),
      ukm_source_id_(ukm_source_id),
      bypass_redirect_checks_(bypass_redirect_checks),
      navigation_id_(navigation_id),
      navigation_response_task_runner_(
          std::move(navigation_response_task_runner)) {}

ContentClientParams::ContentClientParams(ContentClientParams&&) = default;
ContentClientParams& ContentClientParams::operator=(ContentClientParams&&) =
    default;
ContentClientParams::~ContentClientParams() = default;

void ContentClientParams::Run(
    network::URLLoaderFactoryBuilder& factory_builder,
    ContentBrowserClient::URLLoaderFactoryType type,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      browser_context_.get(), frame_.get(), render_process_id_, type,
      request_initiator_.get(), isolation_info_.get(),
      std::move(navigation_id_), std::move(ukm_source_id_), factory_builder,
      header_client, bypass_redirect_checks_.get(), disable_secure_dns,
      factory_override, std::move(navigation_response_task_runner_));
}

namespace {

// Returns (is_navigation, is_download).
std::tuple<bool, bool> GetIsNavigationAndDownload(
    ContentBrowserClient::URLLoaderFactoryType type) {
  switch (type) {
    case ContentBrowserClient::URLLoaderFactoryType::kNavigation:
      return std::make_tuple(/*is_navigation=*/true, /*is_download=*/false);
    case ContentBrowserClient::URLLoaderFactoryType::kDownload:
      return std::make_tuple(/*is_navigation=*/true, /*is_download=*/true);

    case ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource:
    case ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerScript:
    case ContentBrowserClient::URLLoaderFactoryType::kServiceWorkerSubResource:
    case ContentBrowserClient::URLLoaderFactoryType::kWorkerSubResource:
    case ContentBrowserClient::URLLoaderFactoryType::kWorkerMainResource:
      return std::make_tuple(/*is_navigation=*/false, /*is_download=*/false);

    // The following cases are not reached just because `devtools_params` is
    // always `std::nullopt` for the current `Create*` callers.
    // TODO(crbug.com/40947547): Return proper values once non-nullopt
    // `devtools_params` is given.
    case ContentBrowserClient::URLLoaderFactoryType::kPrefetch:
    case ContentBrowserClient::URLLoaderFactoryType::kDevTools:
    case ContentBrowserClient::URLLoaderFactoryType::kEarlyHints:
      NOTREACHED();
  }
}

// `FinishArgs...` is `mojo::PendingReceiver<network::mojom::URLLoaderFactory>`
// when called from `CreateAndConnectToReceiver`, and empty otherwise.
template <typename OutType, typename... FinishArgs>
[[nodiscard]] OutType CreateInternal(
    ContentBrowserClient::URLLoaderFactoryType type,
    TerminalParams terminal_params,
    std::optional<ContentClientParams> content_client_params,
    std::optional<devtools_instrumentation::WillCreateURLLoaderFactoryParams>
        devtools_params,
    FinishArgs... finish_args) {
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  network::mojom::URLLoaderFactoryOverridePtr factory_override;
  auto* factory_override_ptr =
      terminal_params.factory_override_option() == FactoryOverrideOption::kAllow
          ? &factory_override
          : nullptr;
  bool disable_secure_dns = false;

  network::URLLoaderFactoryBuilder factory_builder;

  if (content_client_params) {
    content_client_params->Run(
        factory_builder, type,
        terminal_params.header_client_option() == HeaderClientOption::kAllow
            ? &header_client
            : nullptr,
        terminal_params.disable_secure_dns_option() ==
                DisableSecureDnsOption::kAllow
            ? &disable_secure_dns
            : nullptr,
        factory_override_ptr);
  }

  if (devtools_params) {
    auto [is_navigation, is_download] = GetIsNavigationAndDownload(type);
    devtools_params->Run(is_navigation, is_download, factory_builder,
                         factory_override_ptr);
  }

  if (auto terminal_url_loader_factory =
          terminal_params.TakeURLLoaderFactory()) {
    if (GetTestingInterceptor()) {
      GetTestingInterceptor().Run(terminal_params.process_id(),
                                  factory_builder);
    }

    return absl::visit(
        [&factory_builder, &finish_args...](auto&& terminal) {
          return std::move(factory_builder)
              .template Finish<OutType>(
                  std::forward<FinishArgs>(finish_args)...,
                  std::move(terminal));
        },
        std::move(*terminal_url_loader_factory));
  }

  if (!header_client && terminal_params.storage_partition()) {
    CHECK(!factory_override);
    CHECK(!disable_secure_dns);
    // `GetTestingInterceptor()` isn't used here because it is anyway used
    // inside `GetURLLoaderFactoryForBrowserProcess()`.
    return std::move(factory_builder)
        .template Finish<OutType>(std::forward<FinishArgs>(finish_args)...,
                                  terminal_params.storage_partition()
                                      ->GetURLLoaderFactoryForBrowserProcess());
  }

  CHECK(terminal_params.network_context());
  auto factory_params = terminal_params.TakeFactoryParams();
  CHECK(factory_params);
  factory_params->header_client = std::move(header_client);
  factory_params->factory_override = std::move(factory_override);
  factory_params->disable_secure_dns = disable_secure_dns;

  if (GetTestingInterceptor()) {
    GetTestingInterceptor().Run(terminal_params.process_id(), factory_builder);
  }

  return std::move(factory_builder)
      .template Finish<OutType>(std::forward<FinishArgs>(finish_args)...,
                                terminal_params.network_context(),
                                std::move(factory_params));
}

}  // namespace

scoped_refptr<network::SharedURLLoaderFactory> Create(
    ContentBrowserClient::URLLoaderFactoryType type,
    TerminalParams terminal_params,
    std::optional<ContentClientParams> content_client_params,
    std::optional<devtools_instrumentation::WillCreateURLLoaderFactoryParams>
        devtools_params) {
  return CreateInternal<scoped_refptr<network::SharedURLLoaderFactory>>(
      type, std::move(terminal_params), std::move(content_client_params),
      std::move(devtools_params));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory> CreatePendingRemote(
    ContentBrowserClient::URLLoaderFactoryType type,
    TerminalParams terminal_params,
    std::optional<ContentClientParams> content_client_params,
    std::optional<devtools_instrumentation::WillCreateURLLoaderFactoryParams>
        devtools_params) {
  return CreateInternal<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
      type, std::move(terminal_params), std::move(content_client_params),
      std::move(devtools_params));
}

void CreateAndConnectToPendingReceiver(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver_to_connect,
    ContentBrowserClient::URLLoaderFactoryType type,
    TerminalParams terminal_params,
    std::optional<ContentClientParams> content_client_params,
    std::optional<devtools_instrumentation::WillCreateURLLoaderFactoryParams>
        devtools_params) {
  CreateInternal<void>(
      type, std::move(terminal_params), std::move(content_client_params),
      std::move(devtools_params), std::move(receiver_to_connect));
}

}  // namespace url_loader_factory
}  // namespace content
