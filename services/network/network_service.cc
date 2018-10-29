// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/certificate_transparency/sth_distributor.h"
#include "components/certificate_transparency/sth_observer.h"
#include "components/os_crypt/os_crypt.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_database.h"
#include "net/cert/ct_log_response_parser.h"
#include "net/cert/signed_tree_head.h"
#include "net/dns/dns_config_overrides.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_util.h"
#include "net/ssl/ssl_key_logger_impl.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/crl_set_distributor.h"
#include "services/network/cross_origin_read_blocking.h"
#include "services/network/net_log_capture_mode_type_converter.h"
#include "services/network/net_log_exporter.h"
#include "services/network/network_context.h"
#include "services/network/network_usage_accumulator.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/url_loader.h"
#include "services/network/url_request_context_builder_mojo.h"

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/cpu.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(IS_CHROMECAST)
#include "components/os_crypt/key_storage_config_linux.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace network {

namespace {

NetworkService* g_network_service = nullptr;

net::NetLog* GetNetLog() {
  static base::NoDestructor<net::NetLog> instance;
  return instance.get();
}

// The interval for calls to NetworkService::UpdateLoadStates
constexpr auto kUpdateLoadStatesInterval =
    base::TimeDelta::FromMilliseconds(250);

std::unique_ptr<net::NetworkChangeNotifier>
CreateNetworkChangeNotifierIfNeeded() {
  // There is a global singleton net::NetworkChangeNotifier if NetworkService
  // is running inside of the browser process.
  if (!net::NetworkChangeNotifier::HasNetworkChangeNotifier()) {
#if defined(OS_ANDROID)
    // On Android, NetworkChangeNotifier objects are always set up in process
    // before NetworkService is run.
    return nullptr;
#elif defined(OS_CHROMEOS) || defined(OS_IOS) || defined(OS_FUCHSIA)
    // ChromeOS has its own implementation of NetworkChangeNotifier that lives
    // outside of //net. iOS doesn't embed //content. Fuchsia doesn't have an
    // implementation yet.
    // TODO(xunjieli): Figure out what to do for these 3 platforms.
    NOTIMPLEMENTED();
    return nullptr;
#endif
    return base::WrapUnique(net::NetworkChangeNotifier::Create());
  }
  return nullptr;
}

std::unique_ptr<net::HostResolver> CreateHostResolver(net::NetLog* net_log) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::unique_ptr<net::HostResolver> host_resolver(
      net::HostResolver::CreateDefaultResolver(net_log));
  if (!command_line.HasSwitch(switches::kHostResolverRules))
    return host_resolver;

  std::unique_ptr<net::MappedHostResolver> remapped_host_resolver(
      new net::MappedHostResolver(std::move(host_resolver)));
  remapped_host_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  return std::move(remapped_host_resolver);
}

// This is duplicated in content/browser/loader/resource_dispatcher_host_impl.cc
bool LoadInfoIsMoreInteresting(const mojom::LoadInfo& a,
                               const mojom::LoadInfo& b) {
  // Set |*_uploading_size| to be the size of the corresponding upload body if
  // it's currently being uploaded.

  uint64_t a_uploading_size = 0;
  if (a.load_state == net::LOAD_STATE_SENDING_REQUEST)
    a_uploading_size = a.upload_size;

  uint64_t b_uploading_size = 0;
  if (b.load_state == net::LOAD_STATE_SENDING_REQUEST)
    b_uploading_size = b.upload_size;

  if (a_uploading_size != b_uploading_size)
    return a_uploading_size > b_uploading_size;

  return a.load_state > b.load_state;
}

}  // namespace

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry,
    mojom::NetworkServiceRequest request,
    net::NetLog* net_log)
    : registry_(std::move(registry)), binding_(this) {
  DCHECK(!g_network_service);
  g_network_service = this;
  // |registry_| is nullptr when an in-process NetworkService is
  // created directly. The latter is done in concert with using
  // CreateNetworkContextWithBuilder to ease the transition to using the
  // network service.
  if (registry_) {
    DCHECK(!request.is_pending());
    registry_->AddInterface<mojom::NetworkService>(
        base::BindRepeating(&NetworkService::Bind, base::Unretained(this)));
  } else if (request.is_pending()) {
    Bind(std::move(request));
  }

#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
  // Make sure OpenSSL is initialized before using it to histogram data.
  crypto::EnsureOpenSSLInit();

  // Measure CPUs with broken NEON units. See https://crbug.com/341598.
  UMA_HISTOGRAM_BOOLEAN("Net.HasBrokenNEON", CRYPTO_has_broken_NEON());
  // Measure Android kernels with missing AT_HWCAP2 auxv fields. See
  // https://crbug.com/boringssl/46.
  UMA_HISTOGRAM_BOOLEAN("Net.NeedsHWCAP2Workaround",
                        CRYPTO_needs_hwcap2_workaround());
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Record this once per session, though the switch is appled on a
  // per-NetworkContext basis.
  UMA_HISTOGRAM_BOOLEAN(
      "Net.Certificate.IgnoreCertificateErrorsSPKIListPresent",
      command_line->HasSwitch(switches::kIgnoreCertificateErrorsSPKIList));

  network_change_manager_ = std::make_unique<NetworkChangeManager>(
      CreateNetworkChangeNotifierIfNeeded());

  if (net_log) {
    net_log_ = net_log;
  } else {
    net_log_ = GetNetLog();
  }

  trace_net_log_observer_.WatchForTraceStart(net_log_);

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_ =
      std::make_unique<net::LoggingNetworkChangeObserver>(net_log_);

  network_quality_estimator_manager_ =
      std::make_unique<NetworkQualityEstimatorManager>(net_log_);

  host_resolver_ = CreateHostResolver(net_log_);

  network_usage_accumulator_ = std::make_unique<NetworkUsageAccumulator>();
  sth_distributor_ =
      std::make_unique<certificate_transparency::STHDistributor>();
  crl_set_distributor_ = std::make_unique<CRLSetDistributor>();
}

NetworkService::~NetworkService() {
  DCHECK_EQ(this, g_network_service);
  g_network_service = nullptr;
  // Destroy owned network contexts.
  DestroyNetworkContexts();

  // All NetworkContexts (Owned and unowned) must have been deleted by this
  // point.
  DCHECK(network_contexts_.empty());

  if (file_net_log_observer_) {
    file_net_log_observer_->StopObserving(nullptr /*polled_data*/,
                                          base::OnceClosure());
  }
  trace_net_log_observer_.StopWatchForTraceStart();
}

void NetworkService::set_os_crypt_is_configured() {
  os_crypt_config_set_ = true;
}

std::unique_ptr<NetworkService> NetworkService::Create(
    mojom::NetworkServiceRequest request,
    net::NetLog* net_log) {
  return std::make_unique<NetworkService>(nullptr, std::move(request), net_log);
}

std::unique_ptr<mojom::NetworkContext>
NetworkService::CreateNetworkContextWithBuilder(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params,
    std::unique_ptr<URLRequestContextBuilderMojo> builder,
    net::URLRequestContext** url_request_context) {
  std::unique_ptr<NetworkContext> network_context =
      std::make_unique<NetworkContext>(this, std::move(request),
                                       std::move(params), std::move(builder));
  *url_request_context = network_context->url_request_context();
  return network_context;
}

void NetworkService::SetHostResolver(
    std::unique_ptr<net::HostResolver> host_resolver) {
  DCHECK(network_contexts_.empty());
  host_resolver_ = std::move(host_resolver);
}

std::unique_ptr<NetworkService> NetworkService::CreateForTesting() {
  return base::WrapUnique(
      new NetworkService(std::make_unique<service_manager::BinderRegistry>()));
}

void NetworkService::RegisterNetworkContext(NetworkContext* network_context) {
  // If IsPrimaryNetworkContext() is true, there must be no other
  // NetworkContexts created yet.
  DCHECK(!network_context->IsPrimaryNetworkContext() ||
         network_contexts_.empty());

  DCHECK_EQ(0u, network_contexts_.count(network_context));
  network_contexts_.insert(network_context);
  if (quic_disabled_)
    network_context->DisableQuic();
}

void NetworkService::DeregisterNetworkContext(NetworkContext* network_context) {
  // If the NetworkContext is bthe primary network context, all other
  // NetworkContexts must already have been destroyed.
  DCHECK(!network_context->IsPrimaryNetworkContext() ||
         network_contexts_.size() == 1);

  DCHECK_EQ(1u, network_contexts_.count(network_context));
  network_contexts_.erase(network_context);
}

void NetworkService::CreateNetLogEntriesForActiveObjects(
    net::NetLog::ThreadSafeObserver* observer) {
  std::set<net::URLRequestContext*> contexts;
  for (NetworkContext* nc : network_contexts_)
    contexts.insert(nc->url_request_context());
  return net::CreateNetLogEntriesForActiveObjects(contexts, observer);
}

void NetworkService::SetClient(mojom::NetworkServiceClientPtr client) {
  client_ = std::move(client);
}

void NetworkService::StartNetLog(base::File file,
                                 mojom::NetLogCaptureMode capture_mode,
                                 base::Value client_constants) {
  DCHECK(client_constants.is_dict());
  std::unique_ptr<base::DictionaryValue> constants = net::GetNetConstants();
  constants->MergeDictionary(&client_constants);

  file_net_log_observer_ = net::FileNetLogObserver::CreateUnboundedPreExisting(
      std::move(file), std::move(constants));
  file_net_log_observer_->StartObserving(
      net_log_, mojo::ConvertTo<net::NetLogCaptureMode>(capture_mode));
}

void NetworkService::SetSSLKeyLogFile(const base::FilePath& file) {
  net::SSLClientSocket::SetSSLKeyLogger(
      std::make_unique<net::SSLKeyLoggerImpl>(file));
}

void NetworkService::CreateNetworkContext(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params) {
  // Only the first created NetworkContext can have |primary_next_context| set
  // to true.
  DCHECK(!params->primary_network_context || network_contexts_.empty());

  owned_network_contexts_.emplace(std::make_unique<NetworkContext>(
      this, std::move(request), std::move(params),
      base::BindOnce(&NetworkService::OnNetworkContextConnectionClosed,
                     base::Unretained(this))));
}

void NetworkService::ConfigureStubHostResolver(
    bool stub_resolver_enabled,
    base::Optional<std::vector<mojom::DnsOverHttpsServerPtr>>
        dns_over_https_servers) {
  // If the stub resolver is not enabled, |dns_over_https_servers| has no
  // effect.
  DCHECK(stub_resolver_enabled || !dns_over_https_servers);
  DCHECK(!dns_over_https_servers || !dns_over_https_servers->empty());

  // Enable or disable the stub resolver, as needed. "DnsClient" is class that
  // implements the stub resolver.
  host_resolver_->SetDnsClientEnabled(stub_resolver_enabled);

  // Configure DNS over HTTPS.
  if (!dns_over_https_servers || dns_over_https_servers.value().empty()) {
    host_resolver_->SetDnsConfigOverrides(net::DnsConfigOverrides());
    return;
  }

  for (auto* network_context : network_contexts_) {
    if (!network_context->IsPrimaryNetworkContext())
      continue;

    host_resolver_->SetRequestContext(network_context->url_request_context());

    net::DnsConfigOverrides overrides;
    overrides.dns_over_https_servers.emplace();
    for (const auto& doh_server : *dns_over_https_servers) {
      overrides.dns_over_https_servers.value().emplace_back(
          doh_server->server_template, doh_server->use_post);
    }
    host_resolver_->SetDnsConfigOverrides(overrides);

    return;
  }

  // Execution should generally not reach this line, but could run into races
  // with teardown, or restarting a crashed network process, that could
  // theoretically result in reaching it.
}

void NetworkService::DisableQuic() {
  quic_disabled_ = true;

  for (auto* network_context : network_contexts_) {
    network_context->DisableQuic();
  }
}

void NetworkService::SetUpHttpAuth(
    mojom::HttpAuthStaticParamsPtr http_auth_static_params) {
  DCHECK(!http_auth_handler_factory_);

  http_auth_handler_factory_ = net::HttpAuthHandlerRegistryFactory::Create(
      host_resolver_.get(), &http_auth_preferences_,
      http_auth_static_params->supported_schemes
#if defined(OS_CHROMEOS)
      ,
      http_auth_static_params->allow_gssapi_library_load
#elif (defined(OS_POSIX) && !defined(OS_ANDROID)) || defined(OS_FUCHSIA)
      ,
      http_auth_static_params->gssapi_library_name
#endif
      );
}

void NetworkService::ConfigureHttpAuthPrefs(
    mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_params) {
  http_auth_preferences_.SetServerWhitelist(
      http_auth_dynamic_params->server_whitelist);
  http_auth_preferences_.SetDelegateWhitelist(
      http_auth_dynamic_params->delegate_whitelist);
  http_auth_preferences_.set_negotiate_disable_cname_lookup(
      http_auth_dynamic_params->negotiate_disable_cname_lookup);
  http_auth_preferences_.set_negotiate_enable_port(
      http_auth_dynamic_params->enable_negotiate_port);

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  http_auth_preferences_.set_ntlm_v2_enabled(
      http_auth_dynamic_params->ntlm_v2_enabled);
#endif

#if defined(OS_ANDROID)
  http_auth_preferences_.set_auth_android_negotiate_account_type(
      http_auth_dynamic_params->android_negotiate_account_type);
#endif
}

void NetworkService::SetRawHeadersAccess(uint32_t process_id, bool allow) {
  DCHECK(process_id);
  if (allow)
    processes_with_raw_headers_access_.insert(process_id);
  else
    processes_with_raw_headers_access_.erase(process_id);
}

bool NetworkService::HasRawHeadersAccess(uint32_t process_id) const {
  // Allow raw headers for browser-initiated requests.
  if (!process_id)
    return true;
  return processes_with_raw_headers_access_.find(process_id) !=
         processes_with_raw_headers_access_.end();
}

net::NetLog* NetworkService::net_log() const {
  return net_log_;
}

void NetworkService::GetNetworkChangeManager(
    mojom::NetworkChangeManagerRequest request) {
  network_change_manager_->AddRequest(std::move(request));
}

void NetworkService::GetNetworkQualityEstimatorManager(
    mojom::NetworkQualityEstimatorManagerRequest request) {
  network_quality_estimator_manager_->AddRequest(std::move(request));
}

void NetworkService::GetTotalNetworkUsages(
    mojom::NetworkService::GetTotalNetworkUsagesCallback callback) {
  std::move(callback).Run(network_usage_accumulator_->GetTotalNetworkUsages());
}

void NetworkService::UpdateSignedTreeHead(const net::ct::SignedTreeHead& sth) {
  sth_distributor_->NewSTHObserved(sth);
}

void NetworkService::UpdateCRLSet(base::span<const uint8_t> crl_set) {
  crl_set_distributor_->OnNewCRLSet(crl_set);
}

void NetworkService::OnCertDBChanged() {
  net::CertDatabase::GetInstance()->NotifyObserversCertDBChanged();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void NetworkService::SetCryptConfig(mojom::CryptConfigPtr crypt_config) {
#if !defined(IS_CHROMECAST)
  DCHECK(!os_crypt_config_set_);
  auto config = std::make_unique<os_crypt::Config>();
  config->store = crypt_config->store;
  config->product_name = crypt_config->product_name;
  config->main_thread_runner = base::ThreadTaskRunnerHandle::Get();
  config->should_use_preference = crypt_config->should_use_preference;
  config->user_data_path = crypt_config->user_data_path;
  OSCrypt::SetConfig(std::move(config));
  os_crypt_config_set_ = true;
#endif
}
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
void NetworkService::SetEncryptionKey(const std::string& encryption_key) {
  OSCrypt::SetRawEncryptionKey(encryption_key);
}
#endif  // OS_MACOSX

void NetworkService::AddCorbExceptionForPlugin(uint32_t process_id) {
  DCHECK_NE(mojom::kBrowserProcessId, process_id);
  CrossOriginReadBlocking::AddExceptionForPlugin(process_id);
}

void NetworkService::RemoveCorbExceptionForPlugin(uint32_t process_id) {
  DCHECK_NE(mojom::kBrowserProcessId, process_id);
  CrossOriginReadBlocking::RemoveExceptionForPlugin(process_id);
}

#if defined(OS_ANDROID)
void NetworkService::OnApplicationStateChange(
    base::android::ApplicationState state) {
  for (auto* network_context : network_contexts_)
    network_context->app_status_listener()->Notify(state);
}
#endif

net::HttpAuthHandlerFactory* NetworkService::GetHttpAuthHandlerFactory() {
  if (!http_auth_handler_factory_) {
    http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault(
        host_resolver_.get(), &http_auth_preferences_);
  }
  return http_auth_handler_factory_.get();
}

void NetworkService::OnBeforeURLRequest() {
  if (base::FeatureList::IsEnabled(features::kNetworkService))
    MaybeStartUpdateLoadInfoTimer();
}

certificate_transparency::STHReporter* NetworkService::sth_reporter() {
  return sth_distributor_.get();
}

void NetworkService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(interface_name, std::move(interface_pipe));
}

void NetworkService::DestroyNetworkContexts() {
  // Delete NetworkContexts. If there's a primary NetworkContext, it must be
  // deleted after all other NetworkContexts, to avoid use-after-frees.
  for (auto it = owned_network_contexts_.begin();
       it != owned_network_contexts_.end();) {
    const auto last = it;
    ++it;
    if (!(*last)->IsPrimaryNetworkContext())
      owned_network_contexts_.erase(last);
  }

  // If DNS over HTTPS is enabled, the HostResolver is currently using the
  // primary NetworkContext to do DNS lookups, so need to tell the HostResolver
  // to stop using DNS over HTTPS before destroying the primary NetworkContext.
  // The SetDnsConfigOverrides() call will will fail any in-progress DNS
  // lookups, but only if there are current config overrides (which there will
  // be if DNS over HTTPS is currently enabled).
  host_resolver_->SetDnsConfigOverrides(net::DnsConfigOverrides());
  host_resolver_->SetRequestContext(nullptr);

  DCHECK_LE(owned_network_contexts_.size(), 1u);
  owned_network_contexts_.clear();
}

void NetworkService::OnNetworkContextConnectionClosed(
    NetworkContext* network_context) {
  if (network_context->IsPrimaryNetworkContext()) {
    DestroyNetworkContexts();
    return;
  }

  auto it = owned_network_contexts_.find(network_context);
  DCHECK(it != owned_network_contexts_.end());
  owned_network_contexts_.erase(it);
}

void NetworkService::MaybeStartUpdateLoadInfoTimer() {
  if (waiting_on_load_state_ack_ || update_load_info_timer_.IsRunning())
    return;

  bool has_loader = false;
  for (auto* network_context : network_contexts_) {
    if (!network_context->url_request_context()->url_requests()->empty()) {
      has_loader = true;
      break;
    }
  }

  if (!has_loader)
    return;

  update_load_info_timer_.Start(FROM_HERE, kUpdateLoadStatesInterval, this,
                                &NetworkService::UpdateLoadInfo);
}

void NetworkService::UpdateLoadInfo() {
  // For requests from the same {process_id, routing_id} pair, pick the most
  // important. For ones from the browser, return all of them.
  std::vector<mojom::LoadInfoPtr> infos;
  std::map<std::pair<uint32_t, uint32_t>, mojom::LoadInfoPtr> frame_infos;

  for (auto* network_context : network_contexts_) {
    for (auto* loader :
         *network_context->url_request_context()->url_requests()) {
      auto* url_loader = URLLoader::ForRequest(*loader);
      if (!url_loader)
        continue;

      auto process_id = url_loader->GetProcessId();
      auto routing_id = url_loader->GetRenderFrameId();
      if (routing_id == static_cast<uint32_t>(MSG_ROUTING_NONE)) {
        // If there is no routing_id, then the browser can't associate this with
        // a page so no need to send.
        continue;
      }

      auto load_info = mojom::LoadInfo::New();
      load_info->process_id = process_id;
      load_info->routing_id = routing_id;
      load_info->host = loader->url().host();
      auto load_state = loader->GetLoadState();
      load_info->load_state = static_cast<uint32_t>(load_state.state);
      load_info->state_param = std::move(load_state.param);
      auto upload_progress = loader->GetUploadProgress();
      load_info->upload_size = upload_progress.size();
      load_info->upload_position = upload_progress.position();

      if (process_id == 0) {
        // Requests from the browser can't be compared to ones from child
        // processes, so send them all without looking for the most interesting.
        infos.push_back(std::move(load_info));
        continue;
      }

      auto key = std::make_pair(process_id, routing_id);
      auto existing = frame_infos.find(key);
      if (existing == frame_infos.end() ||
          LoadInfoIsMoreInteresting(*load_info, *existing->second)) {
        frame_infos[key] = std::move(load_info);
      }
    }
  }

  for (auto& it : frame_infos)
    infos.push_back(std::move(it.second));

  if (infos.empty())
    return;

  DCHECK(!waiting_on_load_state_ack_);
  waiting_on_load_state_ack_ = true;
  client_->OnLoadingStateUpdate(
      std::move(infos), base::BindOnce(&NetworkService::AckUpdateLoadInfo,
                                       base::Unretained(this)));
}

void NetworkService::AckUpdateLoadInfo() {
  DCHECK(waiting_on_load_state_ack_);
  waiting_on_load_state_ack_ = false;
  MaybeStartUpdateLoadInfoTimer();
}

void NetworkService::Bind(mojom::NetworkServiceRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

NetworkService* NetworkService::GetNetworkServiceForTesting() {
  return g_network_service;
}

}  // namespace network
