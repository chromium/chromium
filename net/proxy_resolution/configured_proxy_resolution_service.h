// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace net {

class ConfiguredProxyResolutionRequest;
class DhcpPacFileFetcher;
class NetLog;
class PacFileFetcher;
class ProxyDelegate;
class ProxyResolverFactory;
struct PacFileDataWithSource;

// This class decides which proxy server(s) to use for a particular URL request.
// It uses the given ProxyResolver to evaluate a PAC file, which the
// ConfiguredProxyResolutionService then uses to resolve a proxy.  All proxy
// resolution in this class is based on first getting proxy configurations (ex:
// a PAC URL) from some source and then using these configurations to attempt to
// resolve that proxy.
class NET_EXPORT ConfiguredProxyResolutionService
    : public ProxyResolutionService,
      public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::DNSObserver,
      public ProxyConfigService::Observer {
 public:
  // This interface defines the set of policies for when to poll the PAC
  // script for changes.
  //
  // The polling policy decides what the next poll delay should be in
  // milliseconds. It also decides how to wait for this delay -- either
  // by starting a timer to do the poll at exactly |next_delay_ms|
  // (MODE_USE_TIMER) or by waiting for the first network request issued after
  // |next_delay_ms| (MODE_START_AFTER_ACTIVITY).
  //
  // The timer method is more precise and guarantees that polling happens when
  // it was requested. However it has the disadvantage of causing spurious CPU
  // and network activity. It is a reasonable choice to use for short poll
  // intervals which only happen a couple times.
  //
  // However for repeated timers this will prevent the browser from going
  // idle. MODE_START_AFTER_ACTIVITY solves this problem by only polling in
  // direct response to network activity. The drawback to
  // MODE_START_AFTER_ACTIVITY is since the poll is initiated only after the
  // request is received, the first couple requests initiated after a long
  // period of inactivity will likely see a stale version of the PAC script
  // until the background polling gets a chance to update things.
  class NET_EXPORT_PRIVATE PacPollPolicy {
   public:
    enum Mode {
      MODE_USE_TIMER,
      MODE_START_AFTER_ACTIVITY,
    };

    virtual ~PacPollPolicy() = default;

    // Decides the next poll delay. |current_delay| is the delay used
    // by the preceding poll, or a negative TimeDelta value if determining
    // the delay for the initial poll. |initial_error| is the network error
    // code that the last PAC fetch (or WPAD initialization) failed with,
    // or OK if it completed successfully. Implementations must set
    // |next_delay| to a non-negative value.
    virtual Mode GetNextDelay(int initial_error,
                              base::TimeDelta current_delay,
                              base::TimeDelta* next_delay) const = 0;
  };

  // |net_log| is a possibly nullptr destination to send log events to. It must
  // remain alive for the lifetime of this ConfiguredProxyResolutionService.
  ConfiguredProxyResolutionService(
      std::unique_ptr<ProxyConfigService> config_service,
      std::unique_ptr<ProxyResolverFactory> resolver_factory,
      NetLog* net_log,
      bool quick_check_enabled);

  ConfiguredProxyResolutionService(const ConfiguredProxyResolutionService&) =
      delete;
  ConfiguredProxyResolutionService& operator=(
      const ConfiguredProxyResolutionService&) = delete;

  ~ConfiguredProxyResolutionService() override;

  // ProxyResolutionService
  //
  // We use the three possible proxy access types in the following order,
  // doing fallback if one doesn't work.  See "pac_script_decider.h"
  // for the specifics.
  //   1.  WPAD auto-detection
  //   2.  PAC URL
  //   3.  named proxy
  int ResolveProxy(const GURL& url,
                   const std::string& method,
                   const NetworkAnonymizationKey& network_anonymization_key,
                   ProxyInfo* results,
                   CompletionOnceCallback callback,
                   std::unique_ptr<ProxyResolutionRequest>* request,
                   const NetLogWithSource& net_log) override;

  // ProxyResolutionService
  void ReportSuccess(const ProxyInfo& proxy_info) override;

  // Sets the PacFileFetcher and DhcpPacFileFetcher dependencies. This
  // is needed if the ProxyResolver is of type ProxyResolverWithoutFetch.
  void SetPacFileFetchers(
      std::unique_ptr<PacFileFetcher> pac_file_fetcher,
      std::unique_ptr<DhcpPacFileFetcher> dhcp_pac_file_fetcher);
  PacFileFetcher* GetPacFileFetcher() const;

  // ProxyResolutionService
  void SetProxyDelegate(ProxyDelegate* delegate) override;

  // ProxyResolutionService
  void OnShutdown() override;

  // Returns the last configuration fetched from ProxyConfigService.
  const std::optional<ProxyConfigWithAnnotation>& fetched_config() const {
    return fetched_config_;
  }

  // Returns the current configuration being used by ProxyConfigService.
  const std::optional<ProxyConfigWithAnnotation>& config() const {
    return config_;
  }

  // ProxyResolutionService
  const ProxyRetryInfoMap& proxy_retry_info() const override;

  // ProxyResolutionService
  void ClearBadProxiesCache() override;

  // Forces refetching the proxy configuration, and applying it.
  // This re-does everything from fetching the system configuration,
  // to downloading and testing the PAC files.
  void ForceReloadProxyConfig();

  // ProxyResolutionService
  base::Value::Dict GetProxyNetLogValues() override;

  // ProxyResolutionService
  [[nodiscard]] bool CastToConfiguredProxyResolutionService(
      ConfiguredProxyResolutionService** configured_proxy_resolution_service)
      override;

  // Same as CreateProxyResolutionServiceUsingV8ProxyResolver, except it uses
  // system libraries for evaluating the PAC script if available, otherwise
  // skips proxy autoconfig.
  static std::unique_ptr<ConfiguredProxyResolutionService>
  CreateUsingSystemProxyResolver(
      std::unique_ptr<ProxyConfigService> proxy_config_service,
      NetLog* net_log,
      bool quick_check_enabled);

  // Creates a ConfiguredProxyResolutionService without support for proxy
  // autoconfig.
  static std::unique_ptr<ConfiguredProxyResolutionService>
  CreateWithoutProxyResolver(
      std::unique_ptr<ProxyConfigService> proxy_config_service,
      NetLog* net_log);

  // Convenience methods that creates a proxy service using the
  // specified fixed settings.
  static std::unique_ptr<ConfiguredProxyResolutionService> CreateFixedForTest(
      const ProxyConfigWithAnnotation& pc);
  static std::unique_ptr<ConfiguredProxyResolutionService> CreateFixedForTest(
      const std::string& proxy,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  // Creates a proxy service that uses a DIRECT connection for all requests.
  static std::unique_ptr<ConfiguredProxyResolutionService> CreateDirect();

  // This method is used by tests to create a ConfiguredProxyResolutionService
  // that returns a hardcoded proxy fallback list (|pac_string|) for every URL.
  //
  // |pac_string| is a list of proxy servers, in the format that a PAC script
  // would return it. For example, "PROXY foobar:99; SOCKS fml:2; DIRECT"
  static std::unique_ptr<ConfiguredProxyResolutionService>
  CreateFixedFromPacResultForTest(
      const std::string& pac_string,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  // Same as CreateFixedFromPacResultForTest(), except the resulting ProxyInfo
  // from resolutions will be tagged as having been auto-detected.
  static std::unique_ptr<ConfiguredProxyResolutionService>
  CreateFixedFromAutoDetectedPacResultForTest(
      const std::string& pac_string,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  // This method is used by tests to create a ConfiguredProxyResolutionService
  // that returns a proxy fallback list (|proxy_chain|) for every URL.
  static std::unique_ptr<ConfiguredProxyResolutionService>
  CreateFixedFromProxyChainsForTest(
      const std::vector<ProxyChain>& proxy_chains,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  // This method should only be used by unit tests.
  void set_stall_proxy_auto_config_delay(base::TimeDelta delay) {
    stall_proxy_auto_config_delay_ = delay;
  }

  // This method should only be used by unit tests. Returns the previously
  // active policy.
  static const PacPollPolicy* set_pac_script_poll_policy(
      const PacPollPolicy* policy);

  // This method should only be used by unit tests. Creates an instance
  // of the default internal PacPollPolicy used by
  // ConfiguredProxyResolutionService.
  static std::unique_ptr<PacPollPolicy> CreateDefaultPacPollPolicy();

  bool quick_check_enabled_for_testing() const { return quick_check_enabled_; }

 private:
  friend class ConfiguredProxyResolutionRequest;
  FRIEND_TEST_ALL_PREFIXES(ProxyResolutionServiceTest,
                           UpdateConfigAfterFailedAutodetect);
  FRIEND_TEST_ALL_PREFIXES(ProxyResolutionServiceTest,
                           UpdateConfigFromPACToDirect);
  class InitProxyResolver;
  class PacFileDeciderPoller;

  typedef std::set<raw_ptr<ConfiguredProxyResolutionRequest, SetExperimental>>
      PendingRequests;

  enum State {
    STATE_NONE,
    STATE_WAITING_FOR_PROXY_CONFIG,
    STATE_WAITING_FOR_INIT_PROXY_RESOLVER,
    STATE_READY,
  };

  // We won't always be able to return a good LoadState. For example, the
  // ConfiguredProxyResolutionService can only get this information from the
  // InitProxyResolver, which is not always available.
  bool GetLoadStateIfAvailable(LoadState* load_state) const;

  ProxyResolver* GetProxyResolver() const;

  // Resets all the variables associated with the current proxy configuration,
  // and rewinds the current state to |STATE_NONE|. Returns the previous value
  // of |current_state_|.  If |reset_fetched_config| is true then
  // |fetched_config_| will also be reset, otherwise it will be left as-is.
  // Resetting it means that we will have to re-fetch the configuration from
  // the ProxyConfigService later.
  State ResetProxyConfig(bool reset_fetched_config);

  // Retrieves the current proxy configuration from the ProxyConfigService, and
  // starts initializing for it.
  void ApplyProxyConfigIfAvailable();

  // Callback for when the proxy resolver has been initialized with a
  // PAC script.
  void OnInitProxyResolverComplete(int result);

  // Returns ERR_IO_PENDING if the request cannot be completed synchronously.
  // Otherwise it fills |result| with the proxy information for |url|.
  // Completing synchronously means we don't need to query ProxyResolver.
  int TryToCompleteSynchronously(const GURL& url, ProxyInfo* result);

  // Cancels all of the requests sent to the ProxyResolver. These will be
  // restarted when calling SetReady().
  void SuspendAllPendingRequests();

  // Advances the current state to |STATE_READY|, and resumes any pending
  // requests which had been stalled waiting for initialization to complete.
  void SetReady();

  // Returns true if |pending_requests_| contains |req|.
  bool ContainsPendingRequest(ConfiguredProxyResolutionRequest* req);

  // Removes |req| from the list of pending requests.
  void RemovePendingRequest(ConfiguredProxyResolutionRequest* req);

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(
      const GURL& url,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& method,
      ProxyInfo* result,
      int result_code,
      const NetLogWithSource& net_log);

  // Start initialization using |fetched_config_|.
  void InitializeUsingLastFetchedConfig();

  // Start the initialization skipping past the "decision" phase.
  void InitializeUsingDecidedConfig(
      int decider_result,
      const PacFileDataWithSource& script_data,
      const ProxyConfigWithAnnotation& effective_config);

  // NetworkChangeNotifier::IPAddressObserver
  // When this is called, we re-fetch PAC scripts and re-run WPAD.
  void OnIPAddressChanged() override;

  // NetworkChangeNotifier::DNSObserver
  // We respond as above.
  void OnDNSChanged() override;

  // ProxyConfigService::Observer
  void OnProxyConfigChanged(
      const ProxyConfigWithAnnotation& config,
      ProxyConfigService::ConfigAvailability availability) override;

  // When using a PAC script there isn't a user-configurable ProxyBypassRules to
  // check, as the one from manual settings doesn't apply. However we
  // still check for matches against the implicit bypass rules, to prevent PAC
  // scripts from being able to proxy localhost.
  bool ApplyPacBypassRules(const GURL& url, ProxyInfo* results);

  std::unique_ptr<ProxyConfigService> config_service_;
  std::unique_ptr<ProxyResolverFactory> resolver_factory_;

  // If non-null, the initialized ProxyResolver to use for requests.
  std::unique_ptr<ProxyResolver> resolver_;

  // We store the proxy configuration that was last fetched from the
  // ProxyConfigService, as well as the resulting "effective" configuration.
  // The effective configuration is what we condense the original fetched
  // settings to after testing the various automatic settings (auto-detect
  // and custom PAC url).
  //
  // These are "optional" as their value remains unset while being calculated.
  std::optional<ProxyConfigWithAnnotation> fetched_config_;
  std::optional<ProxyConfigWithAnnotation> config_;

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // Set of pending/inprogress requests.
  PendingRequests pending_requests_;

  // The fetcher to use when downloading PAC scripts for the ProxyResolver.
  // This dependency can be nullptr if our ProxyResolver has no need for
  // external PAC script fetching.
  std::unique_ptr<PacFileFetcher> pac_file_fetcher_;

  // The fetcher to use when attempting to download the most appropriate PAC
  // script configured in DHCP, if any. Can be nullptr if the ProxyResolver has
  // no need for DHCP PAC script fetching.
  std::unique_ptr<DhcpPacFileFetcher> dhcp_pac_file_fetcher_;

  // Helper to download the PAC script (wpad + custom) and apply fallback rules.
  //
  // Note that the declaration is important here: |pac_file_fetcher_| and
  // |proxy_resolver_| must outlive |init_proxy_resolver_|.
  std::unique_ptr<InitProxyResolver> init_proxy_resolver_;

  // Helper to poll the PAC script for changes.
  std::unique_ptr<PacFileDeciderPoller> script_poller_;

  State current_state_ = STATE_NONE;

  // Either OK or an ERR_* value indicating that a permanent error (e.g.
  // failed to fetch the PAC script) prevents proxy resolution.
  int permanent_error_ = OK;

  // This is the log where any events generated by |init_proxy_resolver_| are
  // sent to.
  raw_ptr<NetLog> net_log_;

  // The earliest time at which we should run any proxy auto-config. (Used to
  // stall re-configuration following an IP address change).
  base::TimeTicks stall_proxy_autoconfig_until_;

  // The amount of time to stall requests following IP address changes.
  base::TimeDelta stall_proxy_auto_config_delay_;

  // Whether child PacFileDeciders should use QuickCheck
  bool quick_check_enabled_;

  THREAD_CHECKER(thread_checker_);

  raw_ptr<ProxyDelegate> proxy_delegate_ = nullptr;

  // Flag used by |SetReady()| to check if |this| has been deleted by a
  // synchronous callback.
  base::WeakPtrFactory<ConfiguredProxyResolutionService> weak_ptr_factory_{
      this};
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_CONFIGURED_PROXY_RESOLUTION_SERVICE_H_
