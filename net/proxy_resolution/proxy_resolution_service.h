// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLUTION_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "url/gurl.h"

class GURL;

namespace base {
class SequencedTaskRunner;
class TimeDelta;
}  // namespace base

namespace net {

class DhcpPacFileFetcher;
class NetLog;
class PacFileFetcher;
class ProxyDelegate;
class ProxyResolverFactory;
struct PacFileDataWithSource;

// This class can be used to resolve the proxy server to use when loading a
// HTTP(S) URL.  It uses the given ProxyResolver to handle the actual proxy
// resolution.  See ProxyResolverV8 for example.
class NET_EXPORT ProxyResolutionService
    : public NetworkChangeNotifier::IPAddressObserver,
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

    virtual ~PacPollPolicy() {}

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

  // |net_log| is a possibly NULL destination to send log events to. It must
  // remain alive for the lifetime of this ProxyResolutionService.
  ProxyResolutionService(std::unique_ptr<ProxyConfigService> config_service,
                         std::unique_ptr<ProxyResolverFactory> resolver_factory,
                         NetLog* net_log);

  ~ProxyResolutionService() override;

  // Used to track proxy resolution requests that complete asynchronously.
  class Request {
   public:
    virtual ~Request() = default;
    virtual LoadState GetLoadState() const = 0;

   protected:
    Request() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  // Determines the appropriate proxy for |url| for a |method| request and
  // stores the result in |results|. If |method| is empty, the caller can expect
  // method independent resolution.
  //
  // Returns ERR_IO_PENDING if the proxy information could not be provided
  // synchronously, to indicate that the result will be available when the
  // callback is run.  The callback is run on the thread that calls
  // ResolveProxy.
  //
  // The caller is responsible for ensuring that |results| and |callback|
  // remain valid until the callback is run or until |request| is cancelled,
  // which occurs when the unique pointer to it is deleted (by leaving scope or
  // otherwise).  |request| must not be NULL.
  //
  // We use the three possible proxy access types in the following order,
  // doing fallback if one doesn't work.  See "pac_script_decider.h"
  // for the specifics.
  //   1.  WPAD auto-detection
  //   2.  PAC URL
  //   3.  named proxy
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  int ResolveProxy(const GURL& url,
                   const std::string& method,
                   const NetworkIsolationKey& network_isolation_key,
                   ProxyInfo* results,
                   CompletionOnceCallback callback,
                   std::unique_ptr<Request>* request,
                   const NetLogWithSource& net_log);

  // Explicitly trigger proxy fallback for the given |results| by updating our
  // list of bad proxies to include the first entry of |results|, and,
  // additional bad proxies (can be none). Will retry after |retry_delay| if
  // positive, and will use the default proxy retry duration otherwise. Proxies
  // marked as bad will not be retried until |retry_delay| has passed. Returns
  // true if there will be at least one proxy remaining in the list after
  // fallback and false otherwise. This method should be used to add proxies to
  // the bad proxy list only for reasons other than a network error.
  bool MarkProxiesAsBadUntil(
      const ProxyInfo& results,
      base::TimeDelta retry_delay,
      const std::vector<ProxyServer>& additional_bad_proxies,
      const NetLogWithSource& net_log);

  // Called to report that the last proxy connection succeeded.  If |proxy_info|
  // has a non empty proxy_retry_info map, the proxies that have been tried (and
  // failed) for this request will be marked as bad.
  void ReportSuccess(const ProxyInfo& proxy_info);

  // Sets the PacFileFetcher and DhcpPacFileFetcher dependencies. This
  // is needed if the ProxyResolver is of type ProxyResolverWithoutFetch.
  void SetPacFileFetchers(
      std::unique_ptr<PacFileFetcher> pac_file_fetcher,
      std::unique_ptr<DhcpPacFileFetcher> dhcp_pac_file_fetcher);
  PacFileFetcher* GetPacFileFetcher() const;

  // Associates a delegate that with this ProxyResolutionService. |delegate|
  // must outlive |this|.
  // TODO(eroman): Specify this as a dependency at construction time rather
  //               than making it a mutable property.
  void SetProxyDelegate(ProxyDelegate* delegate);

  // In builds with DCHECKs enabled, asserts that there isn't already a
  // delegate associated with |this|.
  void AssertNoProxyDelegate() const;

  // Cancels all network requests, and prevents the service from creating new
  // ones.  Must be called before the URLRequestContext the
  // ProxyResolutionService was created with is torn down, if it's torn down
  // before th ProxyResolutionService itself.
  void OnShutdown();

  // Returns the last configuration fetched from ProxyConfigService.
  const base::Optional<ProxyConfigWithAnnotation>& fetched_config() const {
    return fetched_config_;
  }

  // Returns the current configuration being used by ProxyConfigService.
  const base::Optional<ProxyConfigWithAnnotation>& config() const {
    return config_;
  }

  // Returns the map of proxies which have been marked as "bad".
  const ProxyRetryInfoMap& proxy_retry_info() const {
    return proxy_retry_info_;
  }

  // Clears the list of bad proxy servers that has been cached.
  void ClearBadProxiesCache() {
    proxy_retry_info_.clear();
  }

  // Forces refetching the proxy configuration, and applying it.
  // This re-does everything from fetching the system configuration,
  // to downloading and testing the PAC files.
  void ForceReloadProxyConfig();

  // Same as CreateProxyResolutionServiceUsingV8ProxyResolver, except it uses
  // system libraries for evaluating the PAC script if available, otherwise
  // skips proxy autoconfig.
  static std::unique_ptr<ProxyResolutionService> CreateUsingSystemProxyResolver(
      std::unique_ptr<ProxyConfigService> proxy_config_service,
      NetLog* net_log);

  // Creates a ProxyResolutionService without support for proxy autoconfig.
  static std::unique_ptr<ProxyResolutionService> CreateWithoutProxyResolver(
      std::unique_ptr<ProxyConfigService> proxy_config_service,
      NetLog* net_log);

  // Convenience methods that creates a proxy service using the
  // specified fixed settings.
  static std::unique_ptr<ProxyResolutionService> CreateFixed(
      const ProxyConfigWithAnnotation& pc);
  static std::unique_ptr<ProxyResolutionService> CreateFixed(
      const std::string& proxy,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  // Creates a proxy service that uses a DIRECT connection for all requests.
  static std::unique_ptr<ProxyResolutionService> CreateDirect();

  // This method is used by tests to create a ProxyResolutionService that
  // returns a hardcoded proxy fallback list (|pac_string|) for every URL.
  //
  // |pac_string| is a list of proxy servers, in the format that a PAC script
  // would return it. For example, "PROXY foobar:99; SOCKS fml:2; DIRECT"
  static std::unique_ptr<ProxyResolutionService> CreateFixedFromPacResult(
      const std::string& pac_string,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  // Same as CreateFixedFromPacResult(), except the resulting ProxyInfo from
  // resolutions will be tagged as having been auto-detected.
  static std::unique_ptr<ProxyResolutionService>
  CreateFixedFromAutoDetectedPacResult(
      const std::string& pac_string,
      const NetworkTrafficAnnotationTag& traffic_annotation);

  // Creates a config service appropriate for this platform that fetches the
  // system proxy settings. |main_task_runner| is the thread where the consumer
  // of the ProxyConfigService will live.
  //
  // TODO(mmenke): Should this be a member of ProxyConfigService?
  // The ProxyResolutionService may not even be in the same process as the
  // system ProxyConfigService.
  static std::unique_ptr<ProxyConfigService> CreateSystemProxyConfigService(
      const scoped_refptr<base::SequencedTaskRunner>& main_task_runner);

  // This method should only be used by unit tests.
  void set_stall_proxy_auto_config_delay(base::TimeDelta delay) {
    stall_proxy_auto_config_delay_ = delay;
  }

  // This method should only be used by unit tests. Returns the previously
  // active policy.
  static const PacPollPolicy* set_pac_script_poll_policy(
      const PacPollPolicy* policy);

  // This method should only be used by unit tests. Creates an instance
  // of the default internal PacPollPolicy used by ProxyResolutionService.
  static std::unique_ptr<PacPollPolicy> CreateDefaultPacPollPolicy();

  void set_quick_check_enabled(bool value) {
    quick_check_enabled_ = value;
  }
  bool quick_check_enabled_for_testing() const { return quick_check_enabled_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ProxyResolutionServiceTest,
                           UpdateConfigAfterFailedAutodetect);
  FRIEND_TEST_ALL_PREFIXES(ProxyResolutionServiceTest,
                           UpdateConfigFromPACToDirect);
  class InitProxyResolver;
  class PacFileDeciderPoller;
  class RequestImpl;

  typedef std::set<RequestImpl*> PendingRequests;

  enum State {
    STATE_NONE,
    STATE_WAITING_FOR_PROXY_CONFIG,
    STATE_WAITING_FOR_INIT_PROXY_RESOLVER,
    STATE_READY,
  };

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
  bool ContainsPendingRequest(RequestImpl* req);

  // Removes |req| from the list of pending requests.
  void RemovePendingRequest(RequestImpl* req);

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(const GURL& url,
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
  base::Optional<ProxyConfigWithAnnotation> fetched_config_;
  base::Optional<ProxyConfigWithAnnotation> config_;

  // The time when the proxy configuration was last read from the system.
  base::TimeTicks config_last_update_time_;

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // Set of pending/inprogress requests.
  PendingRequests pending_requests_;

  // The fetcher to use when downloading PAC scripts for the ProxyResolver.
  // This dependency can be NULL if our ProxyResolver has no need for
  // external PAC script fetching.
  std::unique_ptr<PacFileFetcher> pac_file_fetcher_;

  // The fetcher to use when attempting to download the most appropriate PAC
  // script configured in DHCP, if any. Can be NULL if the ProxyResolver has
  // no need for DHCP PAC script fetching.
  std::unique_ptr<DhcpPacFileFetcher> dhcp_pac_file_fetcher_;

  // Helper to download the PAC script (wpad + custom) and apply fallback rules.
  //
  // Note that the declaration is important here: |pac_file_fetcher_| and
  // |proxy_resolver_| must outlive |init_proxy_resolver_|.
  std::unique_ptr<InitProxyResolver> init_proxy_resolver_;

  // Helper to poll the PAC script for changes.
  std::unique_ptr<PacFileDeciderPoller> script_poller_;

  State current_state_;

  // Either OK or an ERR_* value indicating that a permanent error (e.g.
  // failed to fetch the PAC script) prevents proxy resolution.
  int permanent_error_;

  // This is the log where any events generated by |init_proxy_resolver_| are
  // sent to.
  NetLog* net_log_;

  // The earliest time at which we should run any proxy auto-config. (Used to
  // stall re-configuration following an IP address change).
  base::TimeTicks stall_proxy_autoconfig_until_;

  // The amount of time to stall requests following IP address changes.
  base::TimeDelta stall_proxy_auto_config_delay_;

  // Whether child PacFileDeciders should use QuickCheck
  bool quick_check_enabled_;

  THREAD_CHECKER(thread_checker_);

  ProxyDelegate* proxy_delegate_ = nullptr;

  // Flag used by |SetReady()| to check if |this| has been deleted by a
  // synchronous callback.
  base::WeakPtrFactory<ProxyResolutionService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProxyResolutionService);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLUTION_SERVICE_H_
