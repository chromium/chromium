// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_manager.h"

#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/linked_list.h"
#include "base/debug/debugger.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/base/request_priority.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/base/url_util.h"
#include "net/dns/dns_alias_utility.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_response_result_extractor.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_dns_task.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/host_resolver_manager_job.h"
#include "net/dns/host_resolver_manager_request_impl.h"
#include "net/dns/host_resolver_manager_service_endpoint_request_impl.h"
#include "net/dns/host_resolver_mdns_listener_impl.h"
#include "net/dns/host_resolver_mdns_task.h"
#include "net/dns/host_resolver_nat64_task.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/httpssvc_metrics.h"
#include "net/dns/loopback_only.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/public/util.h"
#include "net/dns/record_parsed.h"
#include "net/dns/resolve_context.h"
#include "net/dns/test_dns_config_service.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_MDNS)
#include "net/dns/mdns_client_impl.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <Winsock2.h>
#include "net/base/winsock_init.h"
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <net/if.h>
#include "net/base/sys_addrinfo.h"
#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include <ifaddrs.h>
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

namespace net {

namespace {

// Limit the size of hostnames that will be resolved to combat issues in
// some platform's resolvers.
const size_t kMaxHostLength = 4096;

// Time between IPv6 probes, i.e. for how long results of each IPv6 probe are
// cached.
const int kIPv6ProbePeriodMs = 1000;

// Google DNS address used for IPv6 probes.
const uint8_t kIPv6ProbeAddress[] = {0x20, 0x01, 0x48, 0x60, 0x48, 0x60,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x88, 0x88};

// True if |hostname| ends with either ".local" or ".local.".
bool ResemblesMulticastDNSName(std::string_view hostname) {
  return hostname.ends_with(".local") || hostname.ends_with(".local.");
}

bool ConfigureAsyncDnsNoFallbackFieldTrial() {
  const bool kDefault = false;

  // Configure the AsyncDns field trial as follows:
  // groups AsyncDnsNoFallbackA and AsyncDnsNoFallbackB: return true,
  // groups AsyncDnsA and AsyncDnsB: return false,
  // groups SystemDnsA and SystemDnsB: return false,
  // otherwise (trial absent): return default.
  std::string group_name = base::FieldTrialList::FindFullName("AsyncDns");
  if (!group_name.empty()) {
    return base::StartsWith(group_name, "AsyncDnsNoFallback",
                            base::CompareCase::INSENSITIVE_ASCII);
  }
  return kDefault;
}

base::Value::Dict NetLogIPv6AvailableParams(bool ipv6_available, bool cached) {
  base::Value::Dict dict;
  dict.Set("ipv6_available", ipv6_available);
  dict.Set("cached", cached);
  return dict;
}

// Maximum of 64 concurrent resolver calls (excluding retries).
// Between 2010 and 2020, the limit was set to 6 because of a report of a broken
// home router that would fail in the presence of more simultaneous queries.
// In 2020, we conducted an experiment to see if this kind of router was still
// present on the Internet, and found no evidence of any remaining issues, so
// we increased the limit to 64 at that time.
const size_t kDefaultMaxSystemTasks = 64u;

PrioritizedDispatcher::Limits GetDispatcherLimits(
    const HostResolver::ManagerOptions& options) {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES,
                                       options.max_concurrent_resolves);

  // If not using default, do not use the field trial.
  if (limits.total_jobs != HostResolver::ManagerOptions::kDefaultParallelism)
    return limits;

  // Default, without trial is no reserved slots.
  limits.total_jobs = kDefaultMaxSystemTasks;

  // Parallelism is determined by the field trial.
  std::string group =
      base::FieldTrialList::FindFullName("HostResolverDispatch");

  if (group.empty())
    return limits;

  // The format of the group name is a list of non-negative integers separated
  // by ':'. Each of the elements in the list corresponds to an element in
  // |reserved_slots|, except the last one which is the |total_jobs|.
  std::vector<std::string_view> group_parts = base::SplitStringPiece(
      group, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (group_parts.size() != NUM_PRIORITIES + 1) {
    NOTREACHED_IN_MIGRATION();
    return limits;
  }

  std::vector<size_t> parsed(group_parts.size());
  for (size_t i = 0; i < group_parts.size(); ++i) {
    if (!base::StringToSizeT(group_parts[i], &parsed[i])) {
      NOTREACHED_IN_MIGRATION();
      return limits;
    }
  }

  const size_t total_jobs = parsed.back();
  parsed.pop_back();

  const size_t total_reserved_slots =
      std::accumulate(parsed.begin(), parsed.end(), 0u);

  // There must be some unreserved slots available for the all priorities.
  if (total_reserved_slots > total_jobs ||
      (total_reserved_slots == total_jobs && parsed[MINIMUM_PRIORITY] == 0)) {
    NOTREACHED_IN_MIGRATION();
    return limits;
  }

  limits.total_jobs = total_jobs;
  limits.reserved_slots = parsed;
  return limits;
}

base::Value::Dict NetLogResults(const HostCache::Entry& results) {
  base::Value::Dict dict;
  dict.Set("results", results.NetLogParams());
  return dict;
}

std::vector<IPEndPoint> FilterAddresses(std::vector<IPEndPoint> addresses,
                                        DnsQueryTypeSet query_types) {
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));
  DCHECK(!query_types.empty());

  const AddressFamily want_family =
      HostResolver::DnsQueryTypeSetToAddressFamily(query_types);

  if (want_family == ADDRESS_FAMILY_UNSPECIFIED)
    return addresses;

  // Keep only the endpoints that match `want_family`.
  addresses.erase(
      base::ranges::remove_if(
          addresses,
          [want_family](AddressFamily family) { return family != want_family; },
          &IPEndPoint::GetFamily),
      addresses.end());
  return addresses;
}

int GetPortForGloballyReachableCheck() {
  if (!base::FeatureList::IsEnabled(
          features::kUseAlternativePortForGloballyReachableCheck)) {
    return 443;
  }
  return features::kAlternativePortForGloballyReachableCheck.Get();
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DnsClientCapability)
enum class DnsClientCapability {
  kSecureDisabledInsecureDisabled = 0,
  kSecureDisabledInsecureEnabled = 1,
  kSecureEnabledInsecureDisabled = 2,
  kSecureEnabledInsecureEnabled = 3,
  kMaxValue = kSecureEnabledInsecureEnabled,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/net/enums.xml:DnsClientCapability)

void RecordDnsClientCapabilityMetrics(const DnsClient* dns_client) {
  if (!dns_client) {
    return;
  }
  DnsClientCapability capability;
  if (dns_client->CanUseSecureDnsTransactions()) {
    if (dns_client->CanUseInsecureDnsTransactions()) {
      capability = DnsClientCapability::kSecureEnabledInsecureEnabled;
    } else {
      capability = DnsClientCapability::kSecureEnabledInsecureDisabled;
    }
  } else {
    if (dns_client->CanUseInsecureDnsTransactions()) {
      capability = DnsClientCapability::kSecureDisabledInsecureEnabled;
    } else {
      capability = DnsClientCapability::kSecureDisabledInsecureDisabled;
    }
  }
  base::UmaHistogramEnumeration("Net.DNS.DnsConfig.DnsClientCapability",
                                capability);
}
}  // namespace

//-----------------------------------------------------------------------------

bool ResolveLocalHostname(std::string_view host,
                          std::vector<IPEndPoint>* address_list) {
  address_list->clear();
  if (!IsLocalHostname(host))
    return false;

  address_list->emplace_back(IPAddress::IPv6Localhost(), 0);
  address_list->emplace_back(IPAddress::IPv4Localhost(), 0);

  return true;
}

class HostResolverManager::ProbeRequestImpl
    : public HostResolver::ProbeRequest,
      public ResolveContext::DohStatusObserver {
 public:
  ProbeRequestImpl(base::WeakPtr<ResolveContext> context,
                   base::WeakPtr<HostResolverManager> resolver)
      : context_(std::move(context)), resolver_(std::move(resolver)) {}

  ProbeRequestImpl(const ProbeRequestImpl&) = delete;
  ProbeRequestImpl& operator=(const ProbeRequestImpl&) = delete;

  ~ProbeRequestImpl() override {
    // Ensure that observers are deregistered to avoid wasting memory.
    if (context_)
      context_->UnregisterDohStatusObserver(this);
  }

  int Start() override {
    DCHECK(resolver_);
    DCHECK(!runner_);

    if (!context_)
      return ERR_CONTEXT_SHUT_DOWN;

    context_->RegisterDohStatusObserver(this);

    StartRunner(false /* network_change */);
    return ERR_IO_PENDING;
  }

  // ResolveContext::DohStatusObserver
  void OnSessionChanged() override { CancelRunner(); }

  void OnDohServerUnavailable(bool network_change) override {
    // Start the runner asynchronously, as this may trigger reentrant calls into
    // HostResolverManager, which are not allowed during notification handling.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProbeRequestImpl::StartRunner,
                       weak_ptr_factory_.GetWeakPtr(), network_change));
  }

 private:
  void StartRunner(bool network_change) {
    DCHECK(resolver_);
    DCHECK(!resolver_->invalidation_in_progress_);

    if (!context_)
      return;  // Reachable if the context ends before a posted task runs.

    if (!runner_)
      runner_ = resolver_->CreateDohProbeRunner(context_.get());
    if (runner_)
      runner_->Start(network_change);
  }

  void CancelRunner() {
    runner_.reset();

    // Cancel any asynchronous StartRunner() calls.
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  base::WeakPtr<ResolveContext> context_;

  std::unique_ptr<DnsProbeRunner> runner_;
  base::WeakPtr<HostResolverManager> resolver_;

  base::WeakPtrFactory<ProbeRequestImpl> weak_ptr_factory_{this};
};

//-----------------------------------------------------------------------------

HostResolverManager::HostResolverManager(
    const HostResolver::ManagerOptions& options,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier,
    NetLog* net_log)
    : HostResolverManager(PassKey(),
                          options,
                          system_dns_config_notifier,
                          handles::kInvalidNetworkHandle,
                          net_log) {}

HostResolverManager::HostResolverManager(
    base::PassKey<HostResolverManager>,
    const HostResolver::ManagerOptions& options,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier,
    handles::NetworkHandle target_network,
    NetLog* net_log)
    : host_resolver_system_params_(nullptr, options.max_system_retry_attempts),
      net_log_(net_log),
      system_dns_config_notifier_(system_dns_config_notifier),
      target_network_(target_network),
      check_ipv6_on_wifi_(options.check_ipv6_on_wifi),
      ipv6_reachability_override_(base::FeatureList::IsEnabled(
          features::kEnableIPv6ReachabilityOverride)),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      https_svcb_options_(
          options.https_svcb_options
              ? *options.https_svcb_options
              : HostResolver::HttpsSvcbOptions::FromFeatures()) {
  PrioritizedDispatcher::Limits job_limits = GetDispatcherLimits(options);
  dispatcher_ = std::make_unique<PrioritizedDispatcher>(job_limits);
  max_queued_jobs_ = job_limits.total_jobs * 100u;

  DCHECK_GE(dispatcher_->num_priorities(), static_cast<size_t>(NUM_PRIORITIES));

#if BUILDFLAG(IS_WIN)
  EnsureWinsockInit();
#endif
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)) || \
    BUILDFLAG(IS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  // Network-bound HostResolverManagers don't need to act on network changes.
  if (!IsBoundToNetwork()) {
    NetworkChangeNotifier::AddIPAddressObserver(this);
    NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }
  if (system_dns_config_notifier_)
    system_dns_config_notifier_->AddObserver(this);
  EnsureSystemHostResolverCallReady();

  auto connection_type =
      IsBoundToNetwork()
          ? NetworkChangeNotifier::GetNetworkConnectionType(target_network)
          : NetworkChangeNotifier::GetConnectionType();
  UpdateConnectionType(connection_type);

#if defined(ENABLE_BUILT_IN_DNS)
  dns_client_ = DnsClient::CreateClient(net_log_);
  dns_client_->SetInsecureEnabled(
      options.insecure_dns_client_enabled,
      options.additional_types_via_insecure_dns_enabled);
  dns_client_->SetConfigOverrides(options.dns_config_overrides);
#else
  DCHECK(options.dns_config_overrides == DnsConfigOverrides());
#endif

  allow_fallback_to_systemtask_ = !ConfigureAsyncDnsNoFallbackFieldTrial();
}

HostResolverManager::~HostResolverManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Prevent the dispatcher from starting new jobs.
  dispatcher_->SetLimitsToZero();
  // It's now safe for Jobs to call KillDnsTask on destruction, because
  // OnJobComplete will not start any new jobs.
  jobs_.clear();

  if (target_network_ == handles::kInvalidNetworkHandle) {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
    NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  }
  if (system_dns_config_notifier_)
    system_dns_config_notifier_->RemoveObserver(this);
}

// static
std::unique_ptr<HostResolverManager>
HostResolverManager::CreateNetworkBoundHostResolverManager(
    const HostResolver::ManagerOptions& options,
    handles::NetworkHandle target_network,
    NetLog* net_log) {
#if BUILDFLAG(IS_ANDROID)
  DCHECK(NetworkChangeNotifier::AreNetworkHandlesSupported());
  return std::make_unique<HostResolverManager>(
      PassKey(), options, nullptr /* system_dns_config_notifier */,
      target_network, net_log);
#else   // !BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
  return nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HostResolverManager::CreateRequest(
    absl::variant<url::SchemeHostPort, HostPortPair> host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters,
    ResolveContext* resolve_context) {
  return CreateRequest(HostResolver::Host(std::move(host)),
                       std::move(network_anonymization_key), std::move(net_log),
                       std::move(optional_parameters), resolve_context);
}

std::unique_ptr<HostResolver::ResolveHostRequest>
HostResolverManager::CreateRequest(
    HostResolver::Host host,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters,
    ResolveContext* resolve_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidation_in_progress_);

  DCHECK_EQ(resolve_context->GetTargetNetwork(), target_network_);
  // ResolveContexts must register (via RegisterResolveContext()) before use to
  // ensure cached data is invalidated on network and configuration changes.
  DCHECK(registered_contexts_.HasObserver(resolve_context));

  return std::make_unique<RequestImpl>(
      std::move(net_log), std::move(host), std::move(network_anonymization_key),
      std::move(optional_parameters), resolve_context->GetWeakPtr(),
      weak_ptr_factory_.GetWeakPtr(), tick_clock_);
}

std::unique_ptr<HostResolver::ProbeRequest>
HostResolverManager::CreateDohProbeRequest(ResolveContext* context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return std::make_unique<ProbeRequestImpl>(context->GetWeakPtr(),
                                            weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<HostResolver::MdnsListener>
HostResolverManager::CreateMdnsListener(const HostPortPair& host,
                                        DnsQueryType query_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(DnsQueryType::UNSPECIFIED, query_type);

  auto listener =
      std::make_unique<HostResolverMdnsListenerImpl>(host, query_type);

  MDnsClient* client;
  int rv = GetOrCreateMdnsClient(&client);

  if (rv == OK) {
    std::unique_ptr<net::MDnsListener> inner_listener = client->CreateListener(
        DnsQueryTypeToQtype(query_type), host.host(), listener.get());
    listener->set_inner_listener(std::move(inner_listener));
  } else {
    listener->set_initialization_error(rv);
  }
  return listener;
}

std::unique_ptr<HostResolver::ServiceEndpointRequest>
HostResolverManager::CreateServiceEndpointRequest(
    url::SchemeHostPort scheme_host_port,
    NetworkAnonymizationKey network_anonymization_key,
    NetLogWithSource net_log,
    ResolveHostParameters parameters,
    ResolveContext* resolve_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidation_in_progress_);
  DCHECK_EQ(resolve_context->GetTargetNetwork(), target_network_);
  if (resolve_context) {
    DCHECK(registered_contexts_.HasObserver(resolve_context));
  }

  return std::make_unique<ServiceEndpointRequestImpl>(
      std::move(scheme_host_port), std::move(network_anonymization_key),
      std::move(net_log), std::move(parameters),
      resolve_context ? resolve_context->GetWeakPtr() : nullptr,
      weak_ptr_factory_.GetWeakPtr(), tick_clock_);
}

void HostResolverManager::SetInsecureDnsClientEnabled(
    bool enabled,
    bool additional_dns_types_enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!dns_client_)
    return;

  bool enabled_before = dns_client_->CanUseInsecureDnsTransactions();
  bool additional_types_before =
      enabled_before && dns_client_->CanQueryAdditionalTypesViaInsecureDns();
  dns_client_->SetInsecureEnabled(enabled, additional_dns_types_enabled);

  // Abort current tasks if `CanUseInsecureDnsTransactions()` changes or if
  // insecure transactions are enabled and
  // `CanQueryAdditionalTypesViaInsecureDns()` changes. Changes to allowing
  // additional types don't matter if insecure transactions are completely
  // disabled.
  if (dns_client_->CanUseInsecureDnsTransactions() != enabled_before ||
      (dns_client_->CanUseInsecureDnsTransactions() &&
       dns_client_->CanQueryAdditionalTypesViaInsecureDns() !=
           additional_types_before)) {
    AbortInsecureDnsTasks(ERR_NETWORK_CHANGED, false /* fallback_only */);
  }
}

base::Value::Dict HostResolverManager::GetDnsConfigAsValue() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return dns_client_ ? dns_client_->GetDnsConfigAsValueForNetLog()
                     : base::Value::Dict();
}

void HostResolverManager::SetDnsConfigOverrides(DnsConfigOverrides overrides) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!dns_client_ && overrides == DnsConfigOverrides())
    return;

  // Not allowed to set overrides if compiled without DnsClient.
  DCHECK(dns_client_);

  bool transactions_allowed_before =
      dns_client_->CanUseSecureDnsTransactions() ||
      dns_client_->CanUseInsecureDnsTransactions();
  bool changed = dns_client_->SetConfigOverrides(std::move(overrides));

  if (changed) {
    NetworkChangeNotifier::TriggerNonSystemDnsChange();

    // Only invalidate cache if new overrides have resulted in a config change.
    InvalidateCaches();

    // Need to update jobs iff transactions were previously allowed because
    // in-progress jobs may be running using a now-invalid configuration.
    if (transactions_allowed_before) {
      UpdateJobsForChangedConfig();
    }
  }
}

void HostResolverManager::RegisterResolveContext(ResolveContext* context) {
  registered_contexts_.AddObserver(context);
  context->InvalidateCachesAndPerSessionData(
      dns_client_ ? dns_client_->GetCurrentSession() : nullptr,
      false /* network_change */);
}

void HostResolverManager::DeregisterResolveContext(
    const ResolveContext* context) {
  registered_contexts_.RemoveObserver(context);

  // Destroy Jobs when their context is closed.
  RemoveAllJobs(context);
}

void HostResolverManager::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void HostResolverManager::SetIPv6ReachabilityOverride(
    bool reachability_override) {
  ipv6_reachability_override_ = reachability_override;
}

void HostResolverManager::SetMaxQueuedJobsForTesting(size_t value) {
  DCHECK_EQ(0u, dispatcher_->num_queued_jobs());
  DCHECK_GE(value, 0u);
  max_queued_jobs_ = value;
}

void HostResolverManager::SetHaveOnlyLoopbackAddresses(bool result) {
  if (result) {
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
  } else {
    additional_resolver_flags_ &= ~HOST_RESOLVER_LOOPBACK_ONLY;
  }
}

void HostResolverManager::SetMdnsSocketFactoryForTesting(
    std::unique_ptr<MDnsSocketFactory> socket_factory) {
  DCHECK(!mdns_client_);
  mdns_socket_factory_ = std::move(socket_factory);
}

void HostResolverManager::SetMdnsClientForTesting(
    std::unique_ptr<MDnsClient> client) {
  mdns_client_ = std::move(client);
}

void HostResolverManager::SetDnsClientForTesting(
    std::unique_ptr<DnsClient> dns_client) {
  DCHECK(dns_client);
  if (dns_client_) {
    if (!dns_client->GetSystemConfigForTesting())
      dns_client->SetSystemConfig(dns_client_->GetSystemConfigForTesting());
    dns_client->SetConfigOverrides(dns_client_->GetConfigOverridesForTesting());
  }
  dns_client_ = std::move(dns_client);
  // Inform `registered_contexts_` of the new `DnsClient`.
  InvalidateCaches();
}

void HostResolverManager::SetLastIPv6ProbeResultForTesting(
    bool last_ipv6_probe_result) {
  SetLastIPv6ProbeResult(last_ipv6_probe_result);
}

// static
bool HostResolverManager::IsLocalTask(TaskType task) {
  switch (task) {
    case TaskType::SECURE_CACHE_LOOKUP:
    case TaskType::INSECURE_CACHE_LOOKUP:
    case TaskType::CACHE_LOOKUP:
    case TaskType::CONFIG_PRESET:
    case TaskType::HOSTS:
      return true;
    default:
      return false;
  }
}

void HostResolverManager::InitializeJobKeyAndIPAddress(
    const NetworkAnonymizationKey& network_anonymization_key,
    const ResolveHostParameters& parameters,
    const NetLogWithSource& source_net_log,
    JobKey& out_job_key,
    IPAddress& out_ip_address) {
  out_job_key.network_anonymization_key = network_anonymization_key;
  out_job_key.source = parameters.source;

  const bool is_ip = out_ip_address.AssignFromIPLiteral(
      out_job_key.host.GetHostnameWithoutBrackets());

  out_job_key.secure_dns_mode =
      GetEffectiveSecureDnsMode(parameters.secure_dns_policy);
  out_job_key.flags = HostResolver::ParametersToHostResolverFlags(parameters) |
                      additional_resolver_flags_;

  if (parameters.dns_query_type != DnsQueryType::UNSPECIFIED) {
    out_job_key.query_types = {parameters.dns_query_type};
    return;
  }

  DnsQueryTypeSet effective_types = {DnsQueryType::A, DnsQueryType::AAAA};

  // Disable AAAA queries when we cannot do anything with the results.
  bool use_local_ipv6 = true;
  if (dns_client_) {
    const DnsConfig* config = dns_client_->GetEffectiveConfig();
    if (config) {
      use_local_ipv6 = config->use_local_ipv6;
    }
  }
  // When resolving IPv4 literals, there's no need to probe for IPv6. When
  // resolving IPv6 literals, there's no benefit to artificially limiting our
  // resolution based on a probe. Prior logic ensures that this is an automatic
  // query, so the code requesting the resolution should be amenable to
  // receiving an IPv6 resolution.
  if (!use_local_ipv6 && !is_ip && !last_ipv6_probe_result_ &&
      !ipv6_reachability_override_) {
    out_job_key.flags |= HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
    effective_types.Remove(DnsQueryType::AAAA);
  }

  // Optimistically enable feature-controlled queries. These queries may be
  // skipped at a later point.

  // `https_svcb_options_.enable` has precedence, so if enabled, ignore any
  // other related features.
  if (https_svcb_options_.enable && out_job_key.host.HasScheme()) {
    static const char* const kSchemesForHttpsQuery[] = {
        url::kHttpScheme, url::kHttpsScheme, url::kWsScheme, url::kWssScheme};
    if (base::Contains(kSchemesForHttpsQuery, out_job_key.host.GetScheme())) {
      effective_types.Put(DnsQueryType::HTTPS);
    }
  }

  out_job_key.query_types = effective_types;
}

HostCache::Entry HostResolverManager::ResolveLocally(
    bool only_ipv6_reachable,
    const JobKey& job_key,
    const IPAddress& ip_address,
    ResolveHostParameters::CacheUsage cache_usage,
    SecureDnsPolicy secure_dns_policy,
    HostResolverSource source,
    const NetLogWithSource& source_net_log,
    HostCache* cache,
    std::deque<TaskType>* out_tasks,
    std::optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = std::nullopt;

  CreateTaskSequence(job_key, cache_usage, secure_dns_policy, out_tasks);

  if (!ip_address.IsValid()) {
    // Check that the caller supplied a valid hostname to resolve. For
    // MULTICAST_DNS, we are less restrictive.
    // TODO(ericorth): Control validation based on an explicit flag rather
    // than implicitly based on |source|.
    const bool is_valid_hostname =
        job_key.source == HostResolverSource::MULTICAST_DNS
            ? dns_names_util::IsValidDnsName(job_key.host.GetHostname())
            : IsCanonicalizedHostCompliant(job_key.host.GetHostname());
    if (!is_valid_hostname) {
      return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                              HostCache::Entry::SOURCE_UNKNOWN);
    }
  }

  bool resolve_canonname = job_key.flags & HOST_RESOLVER_CANONNAME;
  bool default_family_due_to_no_ipv6 =
      job_key.flags & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;

  // The result of |getaddrinfo| for empty hosts is inconsistent across systems.
  // On Windows it gives the default interface's address, whereas on Linux it
  // gives an error. We will make it fail on all platforms for consistency.
  if (job_key.host.GetHostname().empty() ||
      job_key.host.GetHostname().size() > kMaxHostLength) {
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  if (ip_address.IsValid()) {
    // Use NAT64Task for IPv4 literal when the network is IPv6 only.
    if (HostResolver::MayUseNAT64ForIPv4Literal(job_key.flags, source,
                                                ip_address) &&
        only_ipv6_reachable) {
      out_tasks->push_front(TaskType::NAT64);
      return HostCache::Entry(ERR_DNS_CACHE_MISS,
                              HostCache::Entry::SOURCE_UNKNOWN);
    }

    return ResolveAsIP(job_key.query_types, resolve_canonname, ip_address);
  }

  // Special-case localhost names, as per the recommendations in
  // https://tools.ietf.org/html/draft-west-let-localhost-be-localhost.
  std::optional<HostCache::Entry> resolved =
      ServeLocalhost(job_key.host.GetHostname(), job_key.query_types,
                     default_family_due_to_no_ipv6);
  if (resolved)
    return resolved.value();

  // Do initial cache lookups.
  while (!out_tasks->empty() && IsLocalTask(out_tasks->front())) {
    TaskType task = out_tasks->front();
    out_tasks->pop_front();
    if (task == TaskType::SECURE_CACHE_LOOKUP ||
        task == TaskType::INSECURE_CACHE_LOOKUP ||
        task == TaskType::CACHE_LOOKUP) {
      bool secure = task == TaskType::SECURE_CACHE_LOOKUP;
      HostCache::Key key = job_key.ToCacheKey(secure);

      bool ignore_secure = task == TaskType::CACHE_LOOKUP;
      resolved = MaybeServeFromCache(cache, key, cache_usage, ignore_secure,
                                     source_net_log, out_stale_info);
      if (resolved) {
        // |MaybeServeFromCache()| will update |*out_stale_info| as needed.
        DCHECK(out_stale_info->has_value());
        source_net_log.AddEvent(
            NetLogEventType::HOST_RESOLVER_MANAGER_CACHE_HIT,
            [&] { return NetLogResults(resolved.value()); });

        // TODO(crbug.com/40178456): Call StartBootstrapFollowup() if the Secure
        // DNS Policy is kBootstrap and the result is not secure.  Note: A naive
        // implementation could cause an infinite loop if |resolved| always
        // expires or is evicted before the followup runs.
        return resolved.value();
      }
      DCHECK(!out_stale_info->has_value());
    } else if (task == TaskType::CONFIG_PRESET) {
      resolved = MaybeReadFromConfig(job_key);
      if (resolved) {
        source_net_log.AddEvent(
            NetLogEventType::HOST_RESOLVER_MANAGER_CONFIG_PRESET_MATCH,
            [&] { return NetLogResults(resolved.value()); });
        StartBootstrapFollowup(job_key, cache, source_net_log);
        return resolved.value();
      }
    } else if (task == TaskType::HOSTS) {
      resolved = ServeFromHosts(job_key.host.GetHostname(), job_key.query_types,
                                default_family_due_to_no_ipv6, *out_tasks);
      if (resolved) {
        source_net_log.AddEvent(
            NetLogEventType::HOST_RESOLVER_MANAGER_HOSTS_HIT,
            [&] { return NetLogResults(resolved.value()); });
        return resolved.value();
      }
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  return HostCache::Entry(ERR_DNS_CACHE_MISS, HostCache::Entry::SOURCE_UNKNOWN);
}

void HostResolverManager::CreateAndStartJob(JobKey key,
                                            std::deque<TaskType> tasks,
                                            RequestImpl* request) {
  DCHECK(!tasks.empty());

  auto jobit = jobs_.find(key);
  Job* job;
  if (jobit == jobs_.end()) {
    job = AddJobWithoutRequest(key, request->parameters().cache_usage,
                               request->host_cache(), std::move(tasks),
                               request->priority(), request->source_net_log());
    job->AddRequest(request);
    job->RunNextTask();
  } else {
    job = jobit->second.get();
    job->AddRequest(request);
  }
}

HostResolverManager::Job* HostResolverManager::AddJobWithoutRequest(
    JobKey key,
    ResolveHostParameters::CacheUsage cache_usage,
    HostCache* host_cache,
    std::deque<TaskType> tasks,
    RequestPriority priority,
    const NetLogWithSource& source_net_log) {
  auto new_job =
      std::make_unique<Job>(weak_ptr_factory_.GetWeakPtr(), key, cache_usage,
                            host_cache, std::move(tasks), priority,
                            source_net_log, tick_clock_, https_svcb_options_);
  auto insert_result = jobs_.emplace(std::move(key), std::move(new_job));
  auto& iterator = insert_result.first;
  bool is_new = insert_result.second;
  DCHECK(is_new);
  auto& job = iterator->second;
  job->OnAddedToJobMap(iterator);
  return job.get();
}

void HostResolverManager::CreateAndStartJobForServiceEndpointRequest(
    JobKey key,
    std::deque<TaskType> tasks,
    ServiceEndpointRequestImpl* request) {
  CHECK(!tasks.empty());

  auto jobit = jobs_.find(key);
  if (jobit == jobs_.end()) {
    Job* job = AddJobWithoutRequest(key, request->parameters().cache_usage,
                                    request->host_cache(), std::move(tasks),
                                    request->priority(), request->net_log());
    job->AddServiceEndpointRequest(request);
    job->RunNextTask();
  } else {
    jobit->second->AddServiceEndpointRequest(request);
  }
}

HostCache::Entry HostResolverManager::ResolveAsIP(DnsQueryTypeSet query_types,
                                                  bool resolve_canonname,
                                                  const IPAddress& ip_address) {
  DCHECK(ip_address.IsValid());
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));

  // IP literals cannot resolve unless the query type is an address query that
  // allows addresses with the same address family as the literal. E.g., don't
  // return IPv6 addresses for IPv4 queries or anything for a non-address query.
  AddressFamily family = GetAddressFamily(ip_address);
  if (!query_types.Has(AddressFamilyToDnsQueryType(family))) {
    return HostCache::Entry(ERR_NAME_NOT_RESOLVED,
                            HostCache::Entry::SOURCE_UNKNOWN);
  }

  std::set<std::string> aliases;
  if (resolve_canonname) {
    aliases = {ip_address.ToString()};
  }
  return HostCache::Entry(OK, {IPEndPoint(ip_address, 0)}, std::move(aliases),
                          HostCache::Entry::SOURCE_UNKNOWN);
}

std::optional<HostCache::Entry> HostResolverManager::MaybeServeFromCache(
    HostCache* cache,
    const HostCache::Key& key,
    ResolveHostParameters::CacheUsage cache_usage,
    bool ignore_secure,
    const NetLogWithSource& source_net_log,
    std::optional<HostCache::EntryStaleness>* out_stale_info) {
  DCHECK(out_stale_info);
  *out_stale_info = std::nullopt;

  if (!cache)
    return std::nullopt;

  if (cache_usage == ResolveHostParameters::CacheUsage::DISALLOWED)
    return std::nullopt;

  // Local-only requests search the cache for non-local-only results.
  HostCache::Key effective_key = key;
  if (effective_key.host_resolver_source == HostResolverSource::LOCAL_ONLY)
    effective_key.host_resolver_source = HostResolverSource::ANY;

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result;
  HostCache::EntryStaleness staleness;
  if (cache_usage == ResolveHostParameters::CacheUsage::STALE_ALLOWED) {
    cache_result = cache->LookupStale(effective_key, tick_clock_->NowTicks(),
                                      &staleness, ignore_secure);
  } else {
    DCHECK(cache_usage == ResolveHostParameters::CacheUsage::ALLOWED);
    cache_result =
        cache->Lookup(effective_key, tick_clock_->NowTicks(), ignore_secure);
    staleness = HostCache::kNotStale;
  }
  if (cache_result) {
    *out_stale_info = std::move(staleness);
    source_net_log.AddEvent(
        NetLogEventType::HOST_RESOLVER_MANAGER_CACHE_HIT,
        [&] { return NetLogResults(cache_result->second); });
    return cache_result->second;
  }
  return std::nullopt;
}

std::optional<HostCache::Entry> HostResolverManager::MaybeReadFromConfig(
    const JobKey& key) {
  DCHECK(HasAddressType(key.query_types));
  if (!key.host.HasScheme()) {
    return std::nullopt;
  }
  std::optional<std::vector<IPEndPoint>> preset_addrs =
      dns_client_->GetPresetAddrs(key.host.AsSchemeHostPort());
  if (!preset_addrs)
    return std::nullopt;

  std::vector<IPEndPoint> filtered_addresses =
      FilterAddresses(std::move(*preset_addrs), key.query_types);
  if (filtered_addresses.empty())
    return std::nullopt;

  return HostCache::Entry(OK, std::move(filtered_addresses), /*aliases=*/{},
                          HostCache::Entry::SOURCE_CONFIG);
}

void HostResolverManager::StartBootstrapFollowup(
    JobKey key,
    HostCache* host_cache,
    const NetLogWithSource& source_net_log) {
  DCHECK_EQ(SecureDnsMode::kOff, key.secure_dns_mode);
  DCHECK(host_cache);

  key.secure_dns_mode = SecureDnsMode::kSecure;
  if (jobs_.count(key) != 0)
    return;

  Job* job = AddJobWithoutRequest(
      key, ResolveHostParameters::CacheUsage::ALLOWED, host_cache,
      {TaskType::SECURE_DNS}, RequestPriority::LOW, source_net_log);
  job->RunNextTask();
}

std::optional<HostCache::Entry> HostResolverManager::ServeFromHosts(
    std::string_view hostname,
    DnsQueryTypeSet query_types,
    bool default_family_due_to_no_ipv6,
    const std::deque<TaskType>& tasks) {
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));
  // Don't attempt a HOSTS lookup if there is no DnsConfig or the HOSTS lookup
  // is going to be done next as part of a system lookup.
  if (!dns_client_ || !HasAddressType(query_types) ||
      (!tasks.empty() && tasks.front() == TaskType::SYSTEM))
    return std::nullopt;
  const DnsHosts* hosts = dns_client_->GetHosts();

  if (!hosts || hosts->empty())
    return std::nullopt;

  // HOSTS lookups are case-insensitive.
  std::string effective_hostname = base::ToLowerASCII(hostname);

  // If |address_family| is ADDRESS_FAMILY_UNSPECIFIED other implementations
  // (glibc and c-ares) return the first matching line. We have more
  // flexibility, but lose implicit ordering.
  // We prefer IPv6 because "happy eyeballs" will fall back to IPv4 if
  // necessary.
  std::vector<IPEndPoint> addresses;
  if (query_types.Has(DnsQueryType::AAAA)) {
    auto it = hosts->find(DnsHostsKey(effective_hostname, ADDRESS_FAMILY_IPV6));
    if (it != hosts->end()) {
      addresses.emplace_back(it->second, 0);
    }
  }

  if (query_types.Has(DnsQueryType::A)) {
    auto it = hosts->find(DnsHostsKey(effective_hostname, ADDRESS_FAMILY_IPV4));
    if (it != hosts->end()) {
      addresses.emplace_back(it->second, 0);
    }
  }

  // If got only loopback addresses and the family was restricted, resolve
  // again, without restrictions. See SystemHostResolverCall for rationale.
  if (default_family_due_to_no_ipv6 &&
      base::ranges::all_of(addresses, &IPAddress::IsIPv4,
                           &IPEndPoint::address) &&
      base::ranges::all_of(addresses, &IPAddress::IsLoopback,
                           &IPEndPoint::address)) {
    query_types.Put(DnsQueryType::AAAA);
    return ServeFromHosts(hostname, query_types, false, tasks);
  }

  if (addresses.empty())
    return std::nullopt;

  return HostCache::Entry(OK, std::move(addresses),
                          /*aliases=*/{}, HostCache::Entry::SOURCE_HOSTS);
}

std::optional<HostCache::Entry> HostResolverManager::ServeLocalhost(
    std::string_view hostname,
    DnsQueryTypeSet query_types,
    bool default_family_due_to_no_ipv6) {
  DCHECK(!query_types.Has(DnsQueryType::UNSPECIFIED));

  std::vector<IPEndPoint> resolved_addresses;
  if (!HasAddressType(query_types) ||
      !ResolveLocalHostname(hostname, &resolved_addresses)) {
    return std::nullopt;
  }

  if (default_family_due_to_no_ipv6 && query_types.Has(DnsQueryType::A) &&
      !query_types.Has(DnsQueryType::AAAA)) {
    // The caller disabled the AAAA query due to lack of detected IPv6 support.
    // (See SystemHostResolverCall for rationale).
    query_types.Put(DnsQueryType::AAAA);
  }
  std::vector<IPEndPoint> filtered_addresses =
      FilterAddresses(std::move(resolved_addresses), query_types);
  return HostCache::Entry(OK, std::move(filtered_addresses), /*aliases=*/{},
                          HostCache::Entry::SOURCE_UNKNOWN);
}

void HostResolverManager::CacheResult(HostCache* cache,
                                      const HostCache::Key& key,
                                      const HostCache::Entry& entry,
                                      base::TimeDelta ttl) {
  // Don't cache an error unless it has a positive TTL.
  if (cache && (entry.error() == OK || ttl.is_positive()))
    cache->Set(key, entry, tick_clock_->NowTicks(), ttl);
}

std::unique_ptr<HostResolverManager::Job> HostResolverManager::RemoveJob(
    JobMap::iterator job_it) {
  CHECK(job_it != jobs_.end(), base::NotFatalUntil::M130);
  DCHECK(job_it->second);
  DCHECK_EQ(1u, jobs_.count(job_it->first));

  std::unique_ptr<Job> job;
  job_it->second.swap(job);
  jobs_.erase(job_it);
  job->OnRemovedFromJobMap();

  return job;
}

SecureDnsMode HostResolverManager::GetEffectiveSecureDnsMode(
    SecureDnsPolicy secure_dns_policy) {
  // Use switch() instead of if() to ensure that all policies are handled.
  switch (secure_dns_policy) {
    case SecureDnsPolicy::kDisable:
    case SecureDnsPolicy::kBootstrap:
      return SecureDnsMode::kOff;
    case SecureDnsPolicy::kAllow:
      break;
  }

  const DnsConfig* config =
      dns_client_ ? dns_client_->GetEffectiveConfig() : nullptr;

  SecureDnsMode secure_dns_mode = SecureDnsMode::kOff;
  if (config) {
    secure_dns_mode = config->secure_dns_mode;
  }
  return secure_dns_mode;
}

bool HostResolverManager::ShouldForceSystemResolverDueToTestOverride() const {
  // If tests have provided a catch-all DNS block and then disabled it, check
  // that we are not at risk of sending queries beyond the local network.
  if (HostResolverProc::GetDefault() && system_resolver_disabled_for_testing_) {
    DCHECK(dns_client_);
    DCHECK(dns_client_->GetEffectiveConfig());
    DCHECK(base::ranges::none_of(dns_client_->GetEffectiveConfig()->nameservers,
                                 &IPAddress::IsPubliclyRoutable,
                                 &IPEndPoint::address))
        << "Test could query a publicly-routable address.";
  }
  return !host_resolver_system_params_.resolver_proc &&
         HostResolverProc::GetDefault() &&
         !system_resolver_disabled_for_testing_;
}

void HostResolverManager::PushDnsTasks(bool system_task_allowed,
                                       SecureDnsMode secure_dns_mode,
                                       bool insecure_tasks_allowed,
                                       bool allow_cache,
                                       bool prioritize_local_lookups,
                                       ResolveContext* resolve_context,
                                       std::deque<TaskType>* out_tasks) {
  DCHECK(dns_client_);
  DCHECK(dns_client_->GetEffectiveConfig());

  // If a catch-all DNS block has been set for unit tests, we shouldn't send
  // DnsTasks. It is still necessary to call this method, however, so that the
  // correct cache tasks for the secure dns mode are added.
  const bool dns_tasks_allowed = !ShouldForceSystemResolverDueToTestOverride();
  // Upgrade the insecure DnsTask depending on the secure dns mode.
  switch (secure_dns_mode) {
    case SecureDnsMode::kSecure:
      DCHECK(!allow_cache ||
             out_tasks->front() == TaskType::SECURE_CACHE_LOOKUP);
      // Policy misconfiguration can put us in secure DNS mode without any DoH
      // servers to query. See https://crbug.com/1326526.
      if (dns_tasks_allowed && dns_client_->CanUseSecureDnsTransactions())
        out_tasks->push_back(TaskType::SECURE_DNS);
      break;
    case SecureDnsMode::kAutomatic:
      DCHECK(!allow_cache || out_tasks->front() == TaskType::CACHE_LOOKUP);
      if (dns_client_->FallbackFromSecureTransactionPreferred(
              resolve_context)) {
        // Don't run a secure DnsTask if there are no available DoH servers.
        if (dns_tasks_allowed && insecure_tasks_allowed)
          out_tasks->push_back(TaskType::DNS);
      } else if (prioritize_local_lookups) {
        // If local lookups are prioritized, the cache should be checked for
        // both secure and insecure results prior to running a secure DnsTask.
        // The task sequence should already contain the appropriate cache task.
        if (dns_tasks_allowed) {
          out_tasks->push_back(TaskType::SECURE_DNS);
          if (insecure_tasks_allowed)
            out_tasks->push_back(TaskType::DNS);
        }
      } else {
        if (allow_cache) {
          // Remove the initial cache lookup task so that the secure and
          // insecure lookups can be separated.
          out_tasks->pop_front();
          out_tasks->push_back(TaskType::SECURE_CACHE_LOOKUP);
        }
        if (dns_tasks_allowed)
          out_tasks->push_back(TaskType::SECURE_DNS);
        if (allow_cache)
          out_tasks->push_back(TaskType::INSECURE_CACHE_LOOKUP);
        if (dns_tasks_allowed && insecure_tasks_allowed)
          out_tasks->push_back(TaskType::DNS);
      }
      break;
    case SecureDnsMode::kOff:
      DCHECK(!allow_cache || IsLocalTask(out_tasks->front()));
      if (dns_tasks_allowed && insecure_tasks_allowed)
        out_tasks->push_back(TaskType::DNS);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  constexpr TaskType kWantTasks[] = {TaskType::DNS, TaskType::SECURE_DNS};
  const bool no_dns_or_secure_tasks =
      base::ranges::find_first_of(*out_tasks, kWantTasks) == out_tasks->end();
  // The system resolver can be used as a fallback for a non-existent or
  // failing DnsTask if allowed by the request parameters.
  if (system_task_allowed &&
      (no_dns_or_secure_tasks || allow_fallback_to_systemtask_))
    out_tasks->push_back(TaskType::SYSTEM);
}

void HostResolverManager::CreateTaskSequence(
    const JobKey& job_key,
    ResolveHostParameters::CacheUsage cache_usage,
    SecureDnsPolicy secure_dns_policy,
    std::deque<TaskType>* out_tasks) {
  DCHECK(out_tasks->empty());

  // A cache lookup should generally be performed first. For jobs involving a
  // DnsTask, this task may be replaced.
  bool allow_cache =
      cache_usage != ResolveHostParameters::CacheUsage::DISALLOWED;
  if (secure_dns_policy == SecureDnsPolicy::kBootstrap) {
    DCHECK_EQ(SecureDnsMode::kOff, job_key.secure_dns_mode);
    if (allow_cache)
      out_tasks->push_front(TaskType::INSECURE_CACHE_LOOKUP);
    out_tasks->push_front(TaskType::CONFIG_PRESET);
    if (allow_cache)
      out_tasks->push_front(TaskType::SECURE_CACHE_LOOKUP);
  } else if (allow_cache) {
    if (job_key.secure_dns_mode == SecureDnsMode::kSecure) {
      out_tasks->push_front(TaskType::SECURE_CACHE_LOOKUP);
    } else {
      out_tasks->push_front(TaskType::CACHE_LOOKUP);
    }
  }
  out_tasks->push_back(TaskType::HOSTS);

  // Determine what type of task a future Job should start.
  bool prioritize_local_lookups =
      cache_usage ==
      HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;

  const bool has_address_type = HasAddressType(job_key.query_types);

  switch (job_key.source) {
    case HostResolverSource::ANY:
      // Records DnsClient capability metrics, only when `source` is ANY. This
      // is to avoid the metrics being skewed by mechanical requests of other
      // source types.
      RecordDnsClientCapabilityMetrics(dns_client_.get());
      // Force address queries with canonname to use HostResolverSystemTask to
      // counter poor CNAME support in DnsTask. See https://crbug.com/872665
      //
      // Otherwise, default to DnsTask (with allowed fallback to
      // HostResolverSystemTask for address queries). But if hostname appears to
      // be an MDNS name (ends in *.local), go with HostResolverSystemTask for
      // address queries and MdnsTask for non- address queries.
      if ((job_key.flags & HOST_RESOLVER_CANONNAME) && has_address_type) {
        out_tasks->push_back(TaskType::SYSTEM);
      } else if (!ResemblesMulticastDNSName(job_key.host.GetHostname())) {
        bool system_task_allowed =
            has_address_type &&
            job_key.secure_dns_mode != SecureDnsMode::kSecure;
        if (dns_client_ && dns_client_->GetEffectiveConfig()) {
          bool insecure_allowed =
              dns_client_->CanUseInsecureDnsTransactions() &&
              !dns_client_->FallbackFromInsecureTransactionPreferred() &&
              (has_address_type ||
               dns_client_->CanQueryAdditionalTypesViaInsecureDns());
          PushDnsTasks(system_task_allowed, job_key.secure_dns_mode,
                       insecure_allowed, allow_cache, prioritize_local_lookups,
                       &*job_key.resolve_context, out_tasks);
        } else if (system_task_allowed) {
          out_tasks->push_back(TaskType::SYSTEM);
        }
      } else if (has_address_type) {
        // For *.local address queries, try the system resolver even if the
        // secure dns mode is SECURE. Public recursive resolvers aren't expected
        // to handle these queries.
        out_tasks->push_back(TaskType::SYSTEM);
      } else {
        out_tasks->push_back(TaskType::MDNS);
      }
      break;
    case HostResolverSource::SYSTEM:
      out_tasks->push_back(TaskType::SYSTEM);
      break;
    case HostResolverSource::DNS:
      if (dns_client_ && dns_client_->GetEffectiveConfig()) {
        bool insecure_allowed =
            dns_client_->CanUseInsecureDnsTransactions() &&
            (has_address_type ||
             dns_client_->CanQueryAdditionalTypesViaInsecureDns());
        PushDnsTasks(false /* system_task_allowed */, job_key.secure_dns_mode,
                     insecure_allowed, allow_cache, prioritize_local_lookups,
                     &*job_key.resolve_context, out_tasks);
      }
      break;
    case HostResolverSource::MULTICAST_DNS:
      out_tasks->push_back(TaskType::MDNS);
      break;
    case HostResolverSource::LOCAL_ONLY:
      // If no external source allowed, a job should not be created or started
      break;
  }

  // `HOST_RESOLVER_CANONNAME` is only supported through system resolution.
  if (job_key.flags & HOST_RESOLVER_CANONNAME) {
    DCHECK(base::ranges::find(*out_tasks, TaskType::DNS) == out_tasks->end());
    DCHECK(base::ranges::find(*out_tasks, TaskType::MDNS) == out_tasks->end());
  }
}

namespace {

bool RequestWillUseWiFi(handles::NetworkHandle network) {
  NetworkChangeNotifier::ConnectionType connection_type;
  if (network == handles::kInvalidNetworkHandle)
    connection_type = NetworkChangeNotifier::GetConnectionType();
  else
    connection_type = NetworkChangeNotifier::GetNetworkConnectionType(network);

  return connection_type == NetworkChangeNotifier::CONNECTION_WIFI;
}

}  // namespace

void HostResolverManager::FinishIPv6ReachabilityCheck(
    CompletionOnceCallback callback,
    int rv) {
  SetLastIPv6ProbeResult((rv == OK) ? true : false);
  std::move(callback).Run(OK);
  if (!ipv6_request_callbacks_.empty()) {
    std::vector<CompletionOnceCallback> tmp_request_callbacks;
    ipv6_request_callbacks_.swap(tmp_request_callbacks);
    for (auto& request_callback : tmp_request_callbacks) {
      std::move(request_callback).Run(OK);
    }
  }
}

int HostResolverManager::StartIPv6ReachabilityCheck(
    const NetLogWithSource& net_log,
    ClientSocketFactory* client_socket_factory,
    CompletionOnceCallback callback) {
  // Don't bother checking if the request will use WiFi and IPv6 is assumed to
  // not work on WiFi.
  if (!check_ipv6_on_wifi_ && RequestWillUseWiFi(target_network_)) {
    probing_ipv6_ = false;
    last_ipv6_probe_result_ = false;
    last_ipv6_probe_time_ = base::TimeTicks();
    return OK;
  }

  if (probing_ipv6_) {
    ipv6_request_callbacks_.push_back(std::move(callback));
    return ERR_IO_PENDING;
  }
  // Cache the result for kIPv6ProbePeriodMs (measured from after
  // StartGloballyReachableCheck() completes).
  int rv = OK;
  bool cached = true;
  if (last_ipv6_probe_time_.is_null() ||
      (tick_clock_->NowTicks() - last_ipv6_probe_time_).InMilliseconds() >
          kIPv6ProbePeriodMs) {
    probing_ipv6_ = true;
    rv = StartGloballyReachableCheck(
        IPAddress(kIPv6ProbeAddress), net_log, client_socket_factory,
        base::BindOnce(&HostResolverManager::FinishIPv6ReachabilityCheck,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    if (rv != ERR_IO_PENDING) {
      SetLastIPv6ProbeResult((rv == OK) ? true : false);
      rv = OK;
    }
    cached = false;
  }
  net_log.AddEvent(
      NetLogEventType::HOST_RESOLVER_MANAGER_IPV6_REACHABILITY_CHECK, [&] {
        return NetLogIPv6AvailableParams(last_ipv6_probe_result_, cached);
      });
  return rv;
}

void HostResolverManager::SetLastIPv6ProbeResult(bool last_ipv6_probe_result) {
  probing_ipv6_ = false;
  last_ipv6_probe_result_ = last_ipv6_probe_result;
  last_ipv6_probe_time_ = tick_clock_->NowTicks();
}

int HostResolverManager::StartGloballyReachableCheck(
    const IPAddress& dest,
    const NetLogWithSource& net_log,
    ClientSocketFactory* client_socket_factory,
    CompletionOnceCallback callback) {
  std::unique_ptr<DatagramClientSocket> probing_socket =
      client_socket_factory->CreateDatagramClientSocket(
          DatagramSocket::DEFAULT_BIND, net_log.net_log(), net_log.source());
  DatagramClientSocket* probing_socket_ptr = probing_socket.get();
  auto refcounted_socket = base::MakeRefCounted<
      base::RefCountedData<std::unique_ptr<DatagramClientSocket>>>(
      std::move(probing_socket));
  int rv = probing_socket_ptr->ConnectAsync(
      IPEndPoint(dest, GetPortForGloballyReachableCheck()),
      base::BindOnce(&HostResolverManager::RunFinishGloballyReachableCheck,
                     weak_ptr_factory_.GetWeakPtr(), refcounted_socket,
                     std::move(callback)));
  if (rv != ERR_IO_PENDING) {
    rv = FinishGloballyReachableCheck(probing_socket_ptr, rv) ? OK : ERR_FAILED;
  }
  return rv;
}

bool HostResolverManager::FinishGloballyReachableCheck(
    DatagramClientSocket* socket,
    int rv) {
  if (rv != OK) {
    return false;
  }
  IPEndPoint endpoint;
  rv = socket->GetLocalAddress(&endpoint);

  if (rv != OK) {
    return false;
  }
  const IPAddress& address = endpoint.address();

  if (address.IsLinkLocal()) {
    return false;
  }

  if (address.IsIPv6()) {
    const uint8_t kTeredoPrefix[] = {0x20, 0x01, 0, 0};
    if (IPAddressStartsWith(address, kTeredoPrefix)) {
      return false;
    }
  }

  return true;
}

void HostResolverManager::RunFinishGloballyReachableCheck(
    scoped_refptr<base::RefCountedData<std::unique_ptr<DatagramClientSocket>>>
        socket,
    CompletionOnceCallback callback,
    int rv) {
  bool is_reachable = FinishGloballyReachableCheck(socket->data.get(), rv);
  std::move(callback).Run(is_reachable ? OK : ERR_FAILED);
}

void HostResolverManager::RunLoopbackProbeJob() {
  RunHaveOnlyLoopbackAddressesJob(
      base::BindOnce(&HostResolverManager::SetHaveOnlyLoopbackAddresses,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HostResolverManager::RemoveAllJobs(const ResolveContext* context) {
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    const JobKey& key = it->first;
    if (&*key.resolve_context == context) {
      RemoveJob(it++);
    } else {
      ++it;
    }
  }
}

void HostResolverManager::AbortJobsWithoutTargetNetwork(bool in_progress_only) {
  // In Abort, a Request callback could spawn new Jobs with matching keys, so
  // first collect and remove all running jobs from `jobs_`.
  std::vector<std::unique_ptr<Job>> jobs_to_abort;
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    Job* job = it->second.get();
    if (!job->HasTargetNetwork() && (!in_progress_only || job->is_running())) {
      jobs_to_abort.push_back(RemoveJob(it++));
    } else {
      ++it;
    }
  }

  // Pause the dispatcher so it won't start any new dispatcher jobs while
  // aborting the old ones.  This is needed so that it won't start the second
  // DnsTransaction for a job in `jobs_to_abort` if the DnsConfig just became
  // invalid.
  PrioritizedDispatcher::Limits limits = dispatcher_->GetLimits();
  dispatcher_->SetLimits(
      PrioritizedDispatcher::Limits(limits.reserved_slots.size(), 0));

  // Life check to bail once `this` is deleted.
  base::WeakPtr<HostResolverManager> self = weak_ptr_factory_.GetWeakPtr();

  // Then Abort them.
  for (size_t i = 0; self.get() && i < jobs_to_abort.size(); ++i) {
    jobs_to_abort[i]->Abort();
  }

  if (self)
    dispatcher_->SetLimits(limits);
}

void HostResolverManager::AbortInsecureDnsTasks(int error, bool fallback_only) {
  // Aborting jobs potentially modifies |jobs_| and may even delete some jobs.
  // Create safe closures of all current jobs.
  std::vector<base::OnceClosure> job_abort_closures;
  for (auto& job : jobs_) {
    job_abort_closures.push_back(
        job.second->GetAbortInsecureDnsTaskClosure(error, fallback_only));
  }

  // Pause the dispatcher so it won't start any new dispatcher jobs while
  // aborting the old ones.  This is needed so that it won't start the second
  // DnsTransaction for a job if the DnsConfig just changed.
  PrioritizedDispatcher::Limits limits = dispatcher_->GetLimits();
  dispatcher_->SetLimits(
      PrioritizedDispatcher::Limits(limits.reserved_slots.size(), 0));

  for (base::OnceClosure& closure : job_abort_closures)
    std::move(closure).Run();

  dispatcher_->SetLimits(limits);
}

// TODO(crbug.com/40641277): Consider removing this and its usage.
void HostResolverManager::TryServingAllJobsFromHosts() {
  if (!dns_client_ || !dns_client_->GetEffectiveConfig())
    return;

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverManager> self = weak_ptr_factory_.GetWeakPtr();

  for (auto it = jobs_.begin(); self.get() && it != jobs_.end();) {
    Job* job = it->second.get();
    ++it;
    // This could remove |job| from |jobs_|, but iterator will remain valid.
    job->ServeFromHosts();
  }
}

void HostResolverManager::OnIPAddressChanged() {
  DCHECK(!IsBoundToNetwork());
  last_ipv6_probe_time_ = base::TimeTicks();
  // Abandon all ProbeJobs.
  probe_weak_ptr_factory_.InvalidateWeakPtrs();
  InvalidateCaches();
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)) || \
    BUILDFLAG(IS_FUCHSIA)
  RunLoopbackProbeJob();
#endif
  AbortJobsWithoutTargetNetwork(true /* in_progress_only */);
  // `this` may be deleted inside AbortJobsWithoutTargetNetwork().
}

void HostResolverManager::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK(!IsBoundToNetwork());
  UpdateConnectionType(type);
}

void HostResolverManager::OnSystemDnsConfigChanged(
    std::optional<DnsConfig> config) {
  DCHECK(!IsBoundToNetwork());
  // If tests have provided a catch-all DNS block and then disabled it, check
  // that we are not at risk of sending queries beyond the local network.
  if (HostResolverProc::GetDefault() && system_resolver_disabled_for_testing_ &&
      config.has_value()) {
    DCHECK(base::ranges::none_of(config->nameservers,
                                 &IPAddress::IsPubliclyRoutable,
                                 &IPEndPoint::address))
        << "Test could query a publicly-routable address.";
  }

  bool changed = false;
  bool transactions_allowed_before = false;
  if (dns_client_) {
    transactions_allowed_before = dns_client_->CanUseSecureDnsTransactions() ||
                                  dns_client_->CanUseInsecureDnsTransactions();
    changed = dns_client_->SetSystemConfig(std::move(config));
  }

  // Always invalidate cache, even if no change is seen.
  InvalidateCaches();

  if (changed) {
    // Need to update jobs iff transactions were previously allowed because
    // in-progress jobs may be running using a now-invalid configuration.
    if (transactions_allowed_before)
      UpdateJobsForChangedConfig();
  }
}

void HostResolverManager::UpdateJobsForChangedConfig() {
  // Life check to bail once `this` is deleted.
  base::WeakPtr<HostResolverManager> self = weak_ptr_factory_.GetWeakPtr();

  // Existing jobs that were set up using the nameservers and secure dns mode
  // from the original config need to be aborted (does not apply to jobs
  // targeting a specific network).
  AbortJobsWithoutTargetNetwork(false /* in_progress_only */);

  // `this` may be deleted inside AbortJobsWithoutTargetNetwork().
  if (self.get())
    TryServingAllJobsFromHosts();
}

void HostResolverManager::OnFallbackResolve(int dns_task_error) {
  DCHECK(dns_client_);
  DCHECK_NE(OK, dns_task_error);

  // Nothing to do if DnsTask is already not preferred.
  if (dns_client_->FallbackFromInsecureTransactionPreferred())
    return;

  dns_client_->IncrementInsecureFallbackFailures();

  // If DnsClient became not preferred, fallback all fallback-allowed insecure
  // DnsTasks to HostResolverSystemTasks.
  if (dns_client_->FallbackFromInsecureTransactionPreferred())
    AbortInsecureDnsTasks(ERR_FAILED, true /* fallback_only */);
}

int HostResolverManager::GetOrCreateMdnsClient(MDnsClient** out_client) {
#if BUILDFLAG(ENABLE_MDNS)
  if (!mdns_client_) {
    if (!mdns_socket_factory_)
      mdns_socket_factory_ = std::make_unique<MDnsSocketFactoryImpl>(net_log_);
    mdns_client_ = MDnsClient::CreateDefault();
  }

  int rv = OK;
  if (!mdns_client_->IsListening())
    rv = mdns_client_->StartListening(mdns_socket_factory_.get());

  DCHECK_NE(ERR_IO_PENDING, rv);
  DCHECK(rv != OK || mdns_client_->IsListening());
  if (rv == OK)
    *out_client = mdns_client_.get();
  return rv;
#else
  // Should not request MDNS resoltuion unless MDNS is enabled.
  NOTREACHED_IN_MIGRATION();
  return ERR_UNEXPECTED;
#endif
}

void HostResolverManager::InvalidateCaches(bool network_change) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidation_in_progress_);

#if DCHECK_IS_ON()
  base::WeakPtr<HostResolverManager> self_ptr = weak_ptr_factory_.GetWeakPtr();
  size_t num_jobs = jobs_.size();
#endif

  invalidation_in_progress_ = true;
  for (auto& context : registered_contexts_) {
    context.InvalidateCachesAndPerSessionData(
        dns_client_ ? dns_client_->GetCurrentSession() : nullptr,
        network_change);
  }
  invalidation_in_progress_ = false;

#if DCHECK_IS_ON()
  // Sanity checks that invalidation does not have reentrancy issues.
  DCHECK(self_ptr);
  DCHECK_EQ(num_jobs, jobs_.size());
#endif
}

void HostResolverManager::UpdateConnectionType(
    NetworkChangeNotifier::ConnectionType type) {
  host_resolver_system_params_.unresponsive_delay =
      GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
          "DnsUnresponsiveDelayMsByConnectionType",
          HostResolverSystemTask::Params::kDnsDefaultUnresponsiveDelay, type);

  // Note that NetworkChangeNotifier always sends a CONNECTION_NONE notification
  // before non-NONE notifications. This check therefore just ensures each
  // connection change notification is handled once and has nothing to do with
  // whether the change is to offline or online.
  if (type == NetworkChangeNotifier::CONNECTION_NONE && dns_client_) {
    dns_client_->ReplaceCurrentSession();
    InvalidateCaches(true /* network_change */);
  }
}

std::unique_ptr<DnsProbeRunner> HostResolverManager::CreateDohProbeRunner(
    ResolveContext* resolve_context) {
  DCHECK(resolve_context);
  DCHECK(registered_contexts_.HasObserver(resolve_context));
  if (!dns_client_ || !dns_client_->CanUseSecureDnsTransactions()) {
    return nullptr;
  }

  return dns_client_->GetTransactionFactory()->CreateDohProbeRunner(
      resolve_context);
}

}  // namespace net
