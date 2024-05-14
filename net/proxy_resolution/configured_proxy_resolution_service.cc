// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/configured_proxy_resolution_service.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/net_errors.h"
#include "net/base/net_info_source_list.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/url_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_util.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/configured_proxy_resolution_request.h"
#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"
#include "net/proxy_resolution/multi_threaded_proxy_resolver.h"
#include "net/proxy_resolution/pac_file_decider.h"
#include "net/proxy_resolution/pac_file_fetcher.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "net/url_request/url_request_context.h"

#if BUILDFLAG(IS_WIN)
#include "net/proxy_resolution/win/proxy_resolver_winhttp.h"
#elif BUILDFLAG(IS_APPLE)
#include "net/proxy_resolution/proxy_resolver_apple.h"
#endif

using base::TimeTicks;

namespace net {

namespace {

const size_t kDefaultNumPacThreads = 4;

// When the IP address changes we don't immediately re-run proxy auto-config.
// Instead, we  wait for |kDelayAfterNetworkChangesMs| before
// attempting to re-valuate proxy auto-config.
//
// During this time window, any resolve requests sent to the
// ConfiguredProxyResolutionService will be queued. Once we have waited the
// required amount of them, the proxy auto-config step will be run, and the
// queued requests resumed.
//
// The reason we play this game is that our signal for detecting network
// changes (NetworkChangeNotifier) may fire *before* the system's networking
// dependencies are fully configured. This is a problem since it means if
// we were to run proxy auto-config right away, it could fail due to spurious
// DNS failures. (see http://crbug.com/50779 for more details.)
//
// By adding the wait window, we give things a better chance to get properly
// set up. Network failures can happen at any time though, so we additionally
// poll the PAC script for changes, which will allow us to recover from these
// sorts of problems.
const int64_t kDelayAfterNetworkChangesMs = 2000;

// This is the default policy for polling the PAC script.
//
// In response to a failure, the poll intervals are:
//    0: 8 seconds  (scheduled on timer)
//    1: 32 seconds
//    2: 2 minutes
//    3+: 4 hours
//
// In response to a success, the poll intervals are:
//    0+: 12 hours
//
// Only the 8 second poll is scheduled on a timer, the rest happen in response
// to network activity (and hence will take longer than the written time).
//
// Explanation for these values:
//
// TODO(eroman): These values are somewhat arbitrary, and need to be tuned
// using some histograms data. Trying to be conservative so as not to break
// existing setups when deployed. A simple exponential retry scheme would be
// more elegant, but places more load on server.
//
// The motivation for trying quickly after failures (8 seconds) is to recover
// from spurious network failures, which are common after the IP address has
// just changed (like DNS failing to resolve). The next 32 second boundary is
// to try and catch other VPN weirdness which anecdotally I have seen take
// 10+ seconds for some users.
//
// The motivation for re-trying after a success is to check for possible
// content changes to the script, or to the WPAD auto-discovery results. We are
// not very aggressive with these checks so as to minimize the risk of
// overloading existing PAC setups. Moreover it is unlikely that PAC scripts
// change very frequently in existing setups. More research is needed to
// motivate what safe values are here, and what other user agents do.
//
// Comparison to other browsers:
//
// In Firefox the PAC URL is re-tried on failures according to
// network.proxy.autoconfig_retry_interval_min and
// network.proxy.autoconfig_retry_interval_max. The defaults are 5 seconds and
// 5 minutes respectively. It doubles the interval at each attempt.
//
// TODO(eroman): Figure out what Internet Explorer does.
class DefaultPollPolicy
    : public ConfiguredProxyResolutionService::PacPollPolicy {
 public:
  DefaultPollPolicy() = default;

  DefaultPollPolicy(const DefaultPollPolicy&) = delete;
  DefaultPollPolicy& operator=(const DefaultPollPolicy&) = delete;

  Mode GetNextDelay(int initial_error,
                    base::TimeDelta current_delay,
                    base::TimeDelta* next_delay) const override {
    if (initial_error != OK) {
      // Re-try policy for failures.
      const int kDelay1Seconds = 8;
      const int kDelay2Seconds = 32;
      const int kDelay3Seconds = 2 * 60;       // 2 minutes
      const int kDelay4Seconds = 4 * 60 * 60;  // 4 Hours

      // Initial poll.
      if (current_delay.is_negative()) {
        *next_delay = base::Seconds(kDelay1Seconds);
        return MODE_USE_TIMER;
      }
      switch (current_delay.InSeconds()) {
        case kDelay1Seconds:
          *next_delay = base::Seconds(kDelay2Seconds);
          return MODE_START_AFTER_ACTIVITY;
        case kDelay2Seconds:
          *next_delay = base::Seconds(kDelay3Seconds);
          return MODE_START_AFTER_ACTIVITY;
        default:
          *next_delay = base::Seconds(kDelay4Seconds);
          return MODE_START_AFTER_ACTIVITY;
      }
    } else {
      // Re-try policy for succeses.
      *next_delay = base::Hours(12);
      return MODE_START_AFTER_ACTIVITY;
    }
  }
};

// Config getter that always returns direct settings.
class ProxyConfigServiceDirect : public ProxyConfigService {
 public:
  // ProxyConfigService implementation:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* config) override {
    *config = ProxyConfigWithAnnotation::CreateDirect();
    return CONFIG_VALID;
  }
};

// Proxy resolver that fails every time.
class ProxyResolverNull : public ProxyResolver {
 public:
  ProxyResolverNull() = default;

  // ProxyResolver implementation.
  int GetProxyForURL(const GURL& url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override {
    return ERR_NOT_IMPLEMENTED;
  }
};

// ProxyResolver that simulates a PAC script which returns
// |pac_string| for every single URL.
class ProxyResolverFromPacString : public ProxyResolver {
 public:
  explicit ProxyResolverFromPacString(const std::string& pac_string)
      : pac_string_(pac_string) {}

  int GetProxyForURL(const GURL& url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override {
    results->UsePacString(pac_string_);
    return OK;
  }

 private:
  const std::string pac_string_;
};

// ProxyResolver that simulates a proxy chain which returns
// |proxy_chain| for every single URL.
class ProxyResolverFromProxyChains : public ProxyResolver {
 public:
  explicit ProxyResolverFromProxyChains(
      const std::vector<ProxyChain>& proxy_chains)
      : proxy_chains_(proxy_chains) {}

  int GetProxyForURL(const GURL& url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override {
    net::ProxyList proxy_list;
    for (const ProxyChain& proxy_chain : proxy_chains_) {
      proxy_list.AddProxyChain(proxy_chain);
    }
    results->UseProxyList(proxy_list);
    return OK;
  }

 private:
  const std::vector<ProxyChain> proxy_chains_;
};

// Creates ProxyResolvers using a platform-specific implementation.
class ProxyResolverFactoryForSystem : public MultiThreadedProxyResolverFactory {
 public:
  explicit ProxyResolverFactoryForSystem(size_t max_num_threads)
      : MultiThreadedProxyResolverFactory(max_num_threads,
                                          false /*expects_pac_bytes*/) {}

  ProxyResolverFactoryForSystem(const ProxyResolverFactoryForSystem&) = delete;
  ProxyResolverFactoryForSystem& operator=(
      const ProxyResolverFactoryForSystem&) = delete;

  std::unique_ptr<ProxyResolverFactory> CreateProxyResolverFactory() override {
#if BUILDFLAG(IS_WIN)
    return std::make_unique<ProxyResolverFactoryWinHttp>();
#elif BUILDFLAG(IS_APPLE)
    return std::make_unique<ProxyResolverFactoryApple>();
#else
    NOTREACHED_IN_MIGRATION();
    return nullptr;
#endif
  }

  static bool IsSupported() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
    return true;
#else
    return false;
#endif
  }
};

class ProxyResolverFactoryForNullResolver : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryForNullResolver() : ProxyResolverFactory(false) {}

  ProxyResolverFactoryForNullResolver(
      const ProxyResolverFactoryForNullResolver&) = delete;
  ProxyResolverFactoryForNullResolver& operator=(
      const ProxyResolverFactoryForNullResolver&) = delete;

  // ProxyResolverFactory overrides.
  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override {
    *resolver = std::make_unique<ProxyResolverNull>();
    return OK;
  }
};

class ProxyResolverFactoryForPacResult : public ProxyResolverFactory {
 public:
  explicit ProxyResolverFactoryForPacResult(const std::string& pac_string)
      : ProxyResolverFactory(false), pac_string_(pac_string) {}

  ProxyResolverFactoryForPacResult(const ProxyResolverFactoryForPacResult&) =
      delete;
  ProxyResolverFactoryForPacResult& operator=(
      const ProxyResolverFactoryForPacResult&) = delete;

  // ProxyResolverFactory override.
  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override {
    *resolver = std::make_unique<ProxyResolverFromPacString>(pac_string_);
    return OK;
  }

 private:
  const std::string pac_string_;
};

class ProxyResolverFactoryForProxyChains : public ProxyResolverFactory {
 public:
  explicit ProxyResolverFactoryForProxyChains(
      const std::vector<ProxyChain>& proxy_chains)
      : ProxyResolverFactory(false), proxy_chains_(proxy_chains) {}

  ProxyResolverFactoryForProxyChains(
      const ProxyResolverFactoryForProxyChains&) = delete;
  ProxyResolverFactoryForProxyChains& operator=(
      const ProxyResolverFactoryForProxyChains&) = delete;

  // ProxyResolverFactory override.
  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override {
    *resolver = std::make_unique<ProxyResolverFromProxyChains>(proxy_chains_);
    return OK;
  }

 private:
  const std::vector<ProxyChain> proxy_chains_;
};

// Returns NetLog parameters describing a proxy configuration change.
base::Value::Dict NetLogProxyConfigChangedParams(
    const std::optional<ProxyConfigWithAnnotation>* old_config,
    const ProxyConfigWithAnnotation* new_config) {
  base::Value::Dict dict;
  // The "old_config" is optional -- the first notification will not have
  // any "previous" configuration.
  if (old_config->has_value())
    dict.Set("old_config", (*old_config)->value().ToValue());
  dict.Set("new_config", new_config->value().ToValue());
  return dict;
}

base::Value::Dict NetLogBadProxyListParams(
    const ProxyRetryInfoMap* retry_info) {
  base::Value::Dict dict;
  base::Value::List list;

  for (const auto& retry_info_pair : *retry_info)
    list.Append(retry_info_pair.first.ToDebugString());
  dict.Set("bad_proxy_list", std::move(list));
  return dict;
}

// Returns NetLog parameters on a successful proxy resolution.
base::Value::Dict NetLogFinishedResolvingProxyParams(const ProxyInfo* result) {
  base::Value::Dict dict;
  dict.Set("proxy_info", result->ToDebugString());
  return dict;
}

// Returns a sanitized copy of |url| which is safe to pass on to a PAC script.
//
// PAC scripts are modelled as being controllable by a network-present
// attacker (since such an attacker can influence the outcome of proxy
// auto-discovery, or modify the contents of insecurely delivered PAC scripts).
//
// As such, it is important that the full path/query of https:// URLs not be
// sent to PAC scripts, since that would give an attacker access to data that
// is ordinarily protected by TLS.
//
// Obscuring the path for http:// URLs isn't being done since it doesn't matter
// for security (attacker can already route traffic through their HTTP proxy
// and see the full URL for http:// requests).
//
// TODO(crbug.com/41412888): Use the same stripping for insecure URL
// schemes.
GURL SanitizeUrl(const GURL& url) {
  DCHECK(url.is_valid());

  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();

  if (url.SchemeIsCryptographic()) {
    replacements.ClearPath();
    replacements.ClearQuery();
  }

  return url.ReplaceComponents(replacements);
}

}  // namespace

// ConfiguredProxyResolutionService::InitProxyResolver
// ----------------------------------

// This glues together two asynchronous steps:
//   (1) PacFileDecider -- try to fetch/validate a sequence of PAC scripts
//       to figure out what we should configure against.
//   (2) Feed the fetched PAC script into the ProxyResolver.
//
// InitProxyResolver is a single-use class which encapsulates cancellation as
// part of its destructor. Start() or StartSkipDecider() should be called just
// once. The instance can be destroyed at any time, and the request will be
// cancelled.

class ConfiguredProxyResolutionService::InitProxyResolver {
 public:
  InitProxyResolver() = default;

  InitProxyResolver(const InitProxyResolver&) = delete;
  InitProxyResolver& operator=(const InitProxyResolver&) = delete;

  // Note that the destruction of PacFileDecider will automatically cancel
  // any outstanding work.
  ~InitProxyResolver() = default;

  // Begins initializing the proxy resolver; calls |callback| when done. A
  // ProxyResolver instance will be created using |proxy_resolver_factory| and
  // assigned to |*proxy_resolver| if the final result is OK.
  int Start(std::unique_ptr<ProxyResolver>* proxy_resolver,
            ProxyResolverFactory* proxy_resolver_factory,
            PacFileFetcher* pac_file_fetcher,
            DhcpPacFileFetcher* dhcp_pac_file_fetcher,
            NetLog* net_log,
            const ProxyConfigWithAnnotation& config,
            base::TimeDelta wait_delay,
            CompletionOnceCallback callback) {
    DCHECK_EQ(State::kNone, next_state_);
    proxy_resolver_ = proxy_resolver;
    proxy_resolver_factory_ = proxy_resolver_factory;

    decider_ = std::make_unique<PacFileDecider>(pac_file_fetcher,
                                                dhcp_pac_file_fetcher, net_log);
    decider_->set_quick_check_enabled(quick_check_enabled_);
    config_ = config;
    wait_delay_ = wait_delay;
    callback_ = std::move(callback);

    next_state_ = State::kDecidePacFile;
    return DoLoop(OK);
  }

  // Similar to Start(), however it skips the PacFileDecider stage. Instead
  // |effective_config|, |decider_result| and |script_data| will be used as the
  // inputs for initializing the ProxyResolver. A ProxyResolver instance will
  // be created using |proxy_resolver_factory| and assigned to
  // |*proxy_resolver| if the final result is OK.
  int StartSkipDecider(std::unique_ptr<ProxyResolver>* proxy_resolver,
                       ProxyResolverFactory* proxy_resolver_factory,
                       const ProxyConfigWithAnnotation& effective_config,
                       int decider_result,
                       const PacFileDataWithSource& script_data,
                       CompletionOnceCallback callback) {
    DCHECK_EQ(State::kNone, next_state_);
    proxy_resolver_ = proxy_resolver;
    proxy_resolver_factory_ = proxy_resolver_factory;

    effective_config_ = effective_config;
    script_data_ = script_data;
    callback_ = std::move(callback);

    if (decider_result != OK)
      return decider_result;

    next_state_ = State::kCreateResolver;
    return DoLoop(OK);
  }

  // Returns the proxy configuration that was selected by PacFileDecider.
  // Should only be called upon completion of the initialization.
  const ProxyConfigWithAnnotation& effective_config() const {
    DCHECK_EQ(State::kNone, next_state_);
    return effective_config_;
  }

  // Returns the PAC script data that was selected by PacFileDecider.
  // Should only be called upon completion of the initialization.
  const PacFileDataWithSource& script_data() {
    DCHECK_EQ(State::kNone, next_state_);
    return script_data_;
  }

  LoadState GetLoadState() const {
    if (next_state_ == State::kDecidePacFileComplete) {
      // In addition to downloading, this state may also include the stall time
      // after network change events (kDelayAfterNetworkChangesMs).
      return LOAD_STATE_DOWNLOADING_PAC_FILE;
    }
    return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
  }

  // This must be called before the HostResolver is torn down.
  void OnShutdown() {
    if (decider_)
      decider_->OnShutdown();
  }

  void set_quick_check_enabled(bool enabled) { quick_check_enabled_ = enabled; }
  bool quick_check_enabled() const { return quick_check_enabled_; }

 private:
  enum class State {
    kNone,
    kDecidePacFile,
    kDecidePacFileComplete,
    kCreateResolver,
    kCreateResolverComplete,
  };

  int DoLoop(int result) {
    DCHECK_NE(next_state_, State::kNone);
    int rv = result;
    do {
      State state = next_state_;
      next_state_ = State::kNone;
      switch (state) {
        case State::kDecidePacFile:
          DCHECK_EQ(OK, rv);
          rv = DoDecidePacFile();
          break;
        case State::kDecidePacFileComplete:
          rv = DoDecidePacFileComplete(rv);
          break;
        case State::kCreateResolver:
          DCHECK_EQ(OK, rv);
          rv = DoCreateResolver();
          break;
        case State::kCreateResolverComplete:
          rv = DoCreateResolverComplete(rv);
          break;
        default:
          NOTREACHED_IN_MIGRATION() << "bad state: " << static_cast<int>(state);
          rv = ERR_UNEXPECTED;
          break;
      }
    } while (rv != ERR_IO_PENDING && next_state_ != State::kNone);
    return rv;
  }

  int DoDecidePacFile() {
    next_state_ = State::kDecidePacFileComplete;

    return decider_->Start(config_, wait_delay_,
                           proxy_resolver_factory_->expects_pac_bytes(),
                           base::BindOnce(&InitProxyResolver::OnIOCompletion,
                                          base::Unretained(this)));
  }

  int DoDecidePacFileComplete(int result) {
    if (result != OK)
      return result;

    effective_config_ = decider_->effective_config();
    script_data_ = decider_->script_data();

    next_state_ = State::kCreateResolver;
    return OK;
  }

  int DoCreateResolver() {
    DCHECK(script_data_.data);
    // TODO(eroman): Should log this latency to the NetLog.
    next_state_ = State::kCreateResolverComplete;
    return proxy_resolver_factory_->CreateProxyResolver(
        script_data_.data, proxy_resolver_,
        base::BindOnce(&InitProxyResolver::OnIOCompletion,
                       base::Unretained(this)),
        &create_resolver_request_);
  }

  int DoCreateResolverComplete(int result) {
    if (result != OK)
      proxy_resolver_->reset();
    return result;
  }

  void OnIOCompletion(int result) {
    DCHECK_NE(State::kNone, next_state_);
    int rv = DoLoop(result);
    if (rv != ERR_IO_PENDING)
      std::move(callback_).Run(result);
  }

  ProxyConfigWithAnnotation config_;
  ProxyConfigWithAnnotation effective_config_;
  PacFileDataWithSource script_data_;
  base::TimeDelta wait_delay_;
  std::unique_ptr<PacFileDecider> decider_;
  raw_ptr<ProxyResolverFactory> proxy_resolver_factory_ = nullptr;
  std::unique_ptr<ProxyResolverFactory::Request> create_resolver_request_;
  raw_ptr<std::unique_ptr<ProxyResolver>> proxy_resolver_ = nullptr;
  CompletionOnceCallback callback_;
  State next_state_ = State::kNone;
  bool quick_check_enabled_ = true;
};

// ConfiguredProxyResolutionService::PacFileDeciderPoller
// ---------------------------

// This helper class encapsulates the logic to schedule and run periodic
// background checks to see if the PAC script (or effective proxy configuration)
// has changed. If a change is detected, then the caller will be notified via
// the ChangeCallback.
class ConfiguredProxyResolutionService::PacFileDeciderPoller {
 public:
  typedef base::RepeatingCallback<
      void(int, const PacFileDataWithSource&, const ProxyConfigWithAnnotation&)>
      ChangeCallback;

  // Builds a poller helper, and starts polling for updates. Whenever a change
  // is observed, |callback| will be invoked with the details.
  //
  //   |config| specifies the (unresolved) proxy configuration to poll.
  //   |proxy_resolver_expects_pac_bytes| the type of proxy resolver we expect
  //                                      to use the resulting script data with
  //                                      (so it can choose the right format).
  //   |pac_file_fetcher| this pointer must remain alive throughout our
  //                      lifetime. It is the dependency that will be used
  //                      for downloading PAC files.
  //   |dhcp_pac_file_fetcher| similar to |pac_file_fetcher|, but for
  //                           he DHCP dependency.
  //   |init_net_error| This is the initial network error (possibly success)
  //                    encountered by the first PAC fetch attempt. We use it
  //                    to schedule updates more aggressively if the initial
  //                    fetch resulted in an error.
  //   |init_script_data| the initial script data from the PAC fetch attempt.
  //                      This is the baseline used to determine when the
  //                      script's contents have changed.
  //   |net_log| the NetLog to log progress into.
  PacFileDeciderPoller(ChangeCallback callback,
                       const ProxyConfigWithAnnotation& config,
                       bool proxy_resolver_expects_pac_bytes,
                       PacFileFetcher* pac_file_fetcher,
                       DhcpPacFileFetcher* dhcp_pac_file_fetcher,
                       int init_net_error,
                       const PacFileDataWithSource& init_script_data,
                       NetLog* net_log)
      : change_callback_(callback),
        config_(config),
        proxy_resolver_expects_pac_bytes_(proxy_resolver_expects_pac_bytes),
        pac_file_fetcher_(pac_file_fetcher),
        dhcp_pac_file_fetcher_(dhcp_pac_file_fetcher),
        last_error_(init_net_error),
        last_script_data_(init_script_data),
        last_poll_time_(TimeTicks::Now()),
        net_log_(net_log) {
    // Set the initial poll delay.
    next_poll_mode_ = poll_policy()->GetNextDelay(
        last_error_, base::Seconds(-1), &next_poll_delay_);
    TryToStartNextPoll(false);
  }

  PacFileDeciderPoller(const PacFileDeciderPoller&) = delete;
  PacFileDeciderPoller& operator=(const PacFileDeciderPoller&) = delete;

  void OnLazyPoll() {
    // We have just been notified of network activity. Use this opportunity to
    // see if we can start our next poll.
    TryToStartNextPoll(true);
  }

  static const PacPollPolicy* set_policy(const PacPollPolicy* policy) {
    const PacPollPolicy* prev = poll_policy_;
    poll_policy_ = policy;
    return prev;
  }

  void set_quick_check_enabled(bool enabled) { quick_check_enabled_ = enabled; }
  bool quick_check_enabled() const { return quick_check_enabled_; }

 private:
  // Returns the effective poll policy (the one injected by unit-tests, or the
  // default).
  const PacPollPolicy* poll_policy() {
    if (poll_policy_)
      return poll_policy_;
    return &default_poll_policy_;
  }

  void StartPollTimer() {
    DCHECK(!decider_.get());

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PacFileDeciderPoller::DoPoll,
                       weak_factory_.GetWeakPtr()),
        next_poll_delay_);
  }

  void TryToStartNextPoll(bool triggered_by_activity) {
    switch (next_poll_mode_) {
      case PacPollPolicy::MODE_USE_TIMER:
        if (!triggered_by_activity)
          StartPollTimer();
        break;

      case PacPollPolicy::MODE_START_AFTER_ACTIVITY:
        if (triggered_by_activity && !decider_.get()) {
          base::TimeDelta elapsed_time = TimeTicks::Now() - last_poll_time_;
          if (elapsed_time >= next_poll_delay_)
            DoPoll();
        }
        break;
    }
  }

  void DoPoll() {
    last_poll_time_ = TimeTicks::Now();

    // Start the PAC file decider to see if anything has changed.
    decider_ = std::make_unique<PacFileDecider>(
        pac_file_fetcher_, dhcp_pac_file_fetcher_, net_log_);
    decider_->set_quick_check_enabled(quick_check_enabled_);
    int result = decider_->Start(
        config_, base::TimeDelta(), proxy_resolver_expects_pac_bytes_,
        base::BindOnce(&PacFileDeciderPoller::OnPacFileDeciderCompleted,
                       base::Unretained(this)));

    if (result != ERR_IO_PENDING)
      OnPacFileDeciderCompleted(result);
  }

  void OnPacFileDeciderCompleted(int result) {
    if (HasScriptDataChanged(result, decider_->script_data())) {
      // Something has changed, we must notify the
      // ConfiguredProxyResolutionService so it can re-initialize its
      // ProxyResolver. Note that we post a notification task rather than
      // calling it directly -- this is done to avoid an ugly destruction
      // sequence, since |this| might be destroyed as a result of the
      // notification.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &PacFileDeciderPoller::NotifyProxyResolutionServiceOfChange,
              weak_factory_.GetWeakPtr(), result, decider_->script_data(),
              decider_->effective_config()));
      return;
    }

    decider_.reset();

    // Decide when the next poll should take place, and possibly start the
    // next timer.
    next_poll_mode_ = poll_policy()->GetNextDelay(last_error_, next_poll_delay_,
                                                  &next_poll_delay_);
    TryToStartNextPoll(false);
  }

  bool HasScriptDataChanged(int result,
                            const PacFileDataWithSource& script_data) {
    if (result != last_error_) {
      // Something changed -- it was failing before and now it succeeded, or
      // conversely it succeeded before and now it failed. Or it failed in
      // both cases, however the specific failure error codes differ.
      return true;
    }

    if (result != OK) {
      // If it failed last time and failed again with the same error code this
      // time, then nothing has actually changed.
      return false;
    }

    // Otherwise if it succeeded both this time and last time, we need to look
    // closer and see if we ended up downloading different content for the PAC
    // script.
    return !script_data.data->Equals(last_script_data_.data.get()) ||
           (script_data.from_auto_detect != last_script_data_.from_auto_detect);
  }

  void NotifyProxyResolutionServiceOfChange(
      int result,
      const PacFileDataWithSource& script_data,
      const ProxyConfigWithAnnotation& effective_config) {
    // Note that |this| may be deleted after calling into the
    // ConfiguredProxyResolutionService.
    change_callback_.Run(result, script_data, effective_config);
  }

  ChangeCallback change_callback_;
  ProxyConfigWithAnnotation config_;
  bool proxy_resolver_expects_pac_bytes_;
  raw_ptr<PacFileFetcher> pac_file_fetcher_;
  raw_ptr<DhcpPacFileFetcher> dhcp_pac_file_fetcher_;

  int last_error_;
  PacFileDataWithSource last_script_data_;

  std::unique_ptr<PacFileDecider> decider_;
  base::TimeDelta next_poll_delay_;
  PacPollPolicy::Mode next_poll_mode_;

  TimeTicks last_poll_time_;

  const raw_ptr<NetLog> net_log_;

  // Polling policy injected by unit-tests. Otherwise this is nullptr and the
  // default policy will be used.
  static const PacPollPolicy* poll_policy_;

  const DefaultPollPolicy default_poll_policy_;

  bool quick_check_enabled_;

  base::WeakPtrFactory<PacFileDeciderPoller> weak_factory_{this};
};

// static
const ConfiguredProxyResolutionService::PacPollPolicy*
    ConfiguredProxyResolutionService::PacFileDeciderPoller::poll_policy_ =
        nullptr;

// ConfiguredProxyResolutionService
// -----------------------------------------------------

ConfiguredProxyResolutionService::ConfiguredProxyResolutionService(
    std::unique_ptr<ProxyConfigService> config_service,
    std::unique_ptr<ProxyResolverFactory> resolver_factory,
    NetLog* net_log,
    bool quick_check_enabled)
    : config_service_(std::move(config_service)),
      resolver_factory_(std::move(resolver_factory)),
      net_log_(net_log),
      stall_proxy_auto_config_delay_(
          base::Milliseconds(kDelayAfterNetworkChangesMs)),
      quick_check_enabled_(quick_check_enabled) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddDNSObserver(this);
  config_service_->AddObserver(this);
}

// static
std::unique_ptr<ConfiguredProxyResolutionService>
ConfiguredProxyResolutionService::CreateUsingSystemProxyResolver(
    std::unique_ptr<ProxyConfigService> proxy_config_service,
    NetLog* net_log,
    bool quick_check_enabled) {
  DCHECK(proxy_config_service);

  if (!ProxyResolverFactoryForSystem::IsSupported()) {
    VLOG(1) << "PAC support disabled because there is no system implementation";
    return CreateWithoutProxyResolver(std::move(proxy_config_service), net_log);
  }

  std::unique_ptr<ConfiguredProxyResolutionService> proxy_resolution_service =
      std::make_unique<ConfiguredProxyResolutionService>(
          std::move(proxy_config_service),
          std::make_unique<ProxyResolverFactoryForSystem>(
              kDefaultNumPacThreads),
          net_log, quick_check_enabled);
  return proxy_resolution_service;
}

// static
std::unique_ptr<ConfiguredProxyResolutionService>
ConfiguredProxyResolutionService::CreateWithoutProxyResolver(
    std::unique_ptr<ProxyConfigService> proxy_config_service,
    NetLog* net_log) {
  return std::make_unique<ConfiguredProxyResolutionService>(
      std::move(proxy_config_service),
      std::make_unique<ProxyResolverFactoryForNullResolver>(), net_log,
      /*quick_check_enabled=*/false);
}

// static
std::unique_ptr<ConfiguredProxyResolutionService>
ConfiguredProxyResolutionService::CreateFixedForTest(
    const ProxyConfigWithAnnotation& pc) {
  // TODO(eroman): This isn't quite right, won't work if |pc| specifies
  //               a PAC script.
  return CreateUsingSystemProxyResolver(
      std::make_unique<ProxyConfigServiceFixed>(pc), nullptr,
      /*quick_check_enabled=*/true);
}

// static
std::unique_ptr<ConfiguredProxyResolutionService>
ConfiguredProxyResolutionService::CreateFixedForTest(
    const std::string& proxy,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(proxy);
  ProxyConfigWithAnnotation annotated_config(proxy_config, traffic_annotation);
  return ConfiguredProxyResolutionService::CreateFixedForTest(annotated_config);
}

// static
std::unique_ptr<ConfiguredProxyResolutionService>
ConfiguredProxyResolutionService::CreateDirect() {
  // Use direct connections.
  return std::make_unique<ConfiguredProxyResolutionService>(
      std::make_unique<ProxyConfigServiceDirect>(),
      std::make_unique<ProxyResolverFactoryForNullResolver>(), nullptr,
      /*quick_check_enabled=*/true);
}

// static
std::unique_ptr<ConfiguredProxyResolutionService>
ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
    const std::string& pac_string,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  // We need the settings to contain an "automatic" setting, otherwise the
  // ProxyResolver dependency we give it will never be used.
  auto proxy_config_service = std::make_unique<ProxyConfigServiceFixed>(
      ProxyConfigWithAnnotation(ProxyConfig::CreateFromCustomPacURL(GURL(
                                    "https://my-pac-script.invalid/wpad.dat")),
                                traffic_annotation));

  return std::make_unique<ConfiguredProxyResolutionService>(
      std::move(proxy_config_service),
      std::make_unique<ProxyResolverFactoryForPacResult>(pac_string), nullptr,
      /*quick_check_enabled=*/true);
}

// static
std::unique_ptr<ConfiguredProxyResolutionService>
ConfiguredProxyResolutionService::CreateFixedFromAutoDetectedPacResultForTest(
    const std::string& pac_string,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  auto proxy_config_service =
      std::make_unique<ProxyConfigServiceFixed>(ProxyConfigWithAnnotation(
          ProxyConfig::CreateAutoDetect(), traffic_annotation));

  return std::make_unique<ConfiguredProxyResolutionService>(
      std::move(proxy_config_service),
      std::make_unique<ProxyResolverFactoryForPacResult>(pac_string), nullptr,
      /*quick_check_enabled=*/true);
}

// static
std::unique_ptr<ConfiguredProxyResolutionService>
ConfiguredProxyResolutionService::CreateFixedFromProxyChainsForTest(
    const std::vector<ProxyChain>& proxy_chains,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  // We need the settings to contain an "automatic" setting, otherwise the
  // ProxyResolver dependency we give it will never be used.
  auto proxy_config_service = std::make_unique<ProxyConfigServiceFixed>(
      ProxyConfigWithAnnotation(ProxyConfig::CreateFromCustomPacURL(GURL(
                                    "https://my-pac-script.invalid/wpad.dat")),
                                traffic_annotation));

  return std::make_unique<ConfiguredProxyResolutionService>(
      std::move(proxy_config_service),
      std::make_unique<ProxyResolverFactoryForProxyChains>(proxy_chains),
      nullptr,
      /*quick_check_enabled=*/true);
}

int ConfiguredProxyResolutionService::ResolveProxy(
    const GURL& raw_url,
    const std::string& method,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* result,
    CompletionOnceCallback callback,
    std::unique_ptr<ProxyResolutionRequest>* out_request,
    const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!callback.is_null());
  DCHECK(out_request);

  net_log.BeginEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);

  // Notify our polling-based dependencies that a resolve is taking place.
  // This way they can schedule their polls in response to network activity.
  config_service_->OnLazyPoll();
  if (script_poller_.get())
    script_poller_->OnLazyPoll();

  if (current_state_ == STATE_NONE)
    ApplyProxyConfigIfAvailable();

  // Sanitize the URL before passing it on to the proxy resolver (i.e. PAC
  // script). The goal is to remove sensitive data (like embedded user names
  // and password), and local data (i.e. reference fragment) which does not need
  // to be disclosed to the resolver.
  GURL url = SanitizeUrl(raw_url);

  // Check if the request can be completed right away. (This is the case when
  // using a direct connection for example).
  int rv = TryToCompleteSynchronously(url, result);
  if (rv != ERR_IO_PENDING) {
    rv = DidFinishResolvingProxy(url, network_anonymization_key, method, result,
                                 rv, net_log);
    return rv;
  }

  auto req = std::make_unique<ConfiguredProxyResolutionRequest>(
      this, url, method, network_anonymization_key, result, std::move(callback),
      net_log);

  if (current_state_ == STATE_READY) {
    // Start the resolve request.
    rv = req->Start();
    if (rv != ERR_IO_PENDING)
      return req->QueryDidCompleteSynchronously(rv);
  } else {
    req->net_log()->BeginEvent(
        NetLogEventType::PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC);
  }

  DCHECK_EQ(ERR_IO_PENDING, rv);
  DCHECK(!ContainsPendingRequest(req.get()));
  pending_requests_.insert(req.get());

  // Completion will be notified through |callback|, unless the caller cancels
  // the request using |out_request|.
  *out_request = std::move(req);
  return rv;  // ERR_IO_PENDING
}

int ConfiguredProxyResolutionService::TryToCompleteSynchronously(
    const GURL& url,
    ProxyInfo* result) {
  DCHECK_NE(STATE_NONE, current_state_);

  if (current_state_ != STATE_READY)
    return ERR_IO_PENDING;  // Still initializing.

  DCHECK(config_);
  // If it was impossible to fetch or parse the PAC script, we cannot complete
  // the request here and bail out.
  if (permanent_error_ != OK) {
    // Before returning the permanent error check if the URL would have been
    // implicitly bypassed.
    if (ApplyPacBypassRules(url, result))
      return OK;
    return permanent_error_;
  }

  if (config_->value().HasAutomaticSettings())
    return ERR_IO_PENDING;  // Must submit the request to the proxy resolver.

  // Use the manual proxy settings.
  config_->value().proxy_rules().Apply(url, result);
  result->set_traffic_annotation(
      MutableNetworkTrafficAnnotationTag(config_->traffic_annotation()));

  return OK;
}

ConfiguredProxyResolutionService::~ConfiguredProxyResolutionService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  NetworkChangeNotifier::RemoveDNSObserver(this);
  config_service_->RemoveObserver(this);

  // Cancel any inprogress requests.
  // This cancels the internal requests, but leaves the responsibility of
  // canceling the high-level Request (by deleting it) to the client.
  // Since |pending_requests_| might be modified in one of the requests'
  // callbacks (if it deletes another request), iterating through the set in a
  // for-loop will not work.
  while (!pending_requests_.empty()) {
    ConfiguredProxyResolutionRequest* req = *pending_requests_.begin();
    req->QueryComplete(ERR_ABORTED);
  }
}

void ConfiguredProxyResolutionService::SuspendAllPendingRequests() {
  for (ConfiguredProxyResolutionRequest* req : pending_requests_) {
    if (req->is_started()) {
      req->CancelResolveJob();

      req->net_log()->BeginEvent(
          NetLogEventType::PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC);
    }
  }
}

void ConfiguredProxyResolutionService::SetReady() {
  DCHECK(!init_proxy_resolver_.get());
  current_state_ = STATE_READY;

  // TODO(lilyhoughton): This is necessary because a callback invoked by
  // |StartAndCompleteCheckingForSynchronous()| might delete |this|.  A better
  // solution would be to disallow synchronous callbacks altogether.
  base::WeakPtr<ConfiguredProxyResolutionService> weak_this =
      weak_ptr_factory_.GetWeakPtr();

  auto pending_requests_copy = pending_requests_;
  for (ConfiguredProxyResolutionRequest* req : pending_requests_copy) {
    if (!ContainsPendingRequest(req))
      continue;

    if (!req->is_started()) {
      req->net_log()->EndEvent(
          NetLogEventType::PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC);

      // Note that we re-check for synchronous completion, in case we are
      // no longer using a ProxyResolver (can happen if we fell-back to manual.)
      req->StartAndCompleteCheckingForSynchronous();
      if (!weak_this)
        return;  // Synchronous callback deleted |this|
    }
  }
}

void ConfiguredProxyResolutionService::ApplyProxyConfigIfAvailable() {
  DCHECK_EQ(STATE_NONE, current_state_);

  config_service_->OnLazyPoll();

  // If we have already fetched the configuration, start applying it.
  if (fetched_config_) {
    InitializeUsingLastFetchedConfig();
    return;
  }

  // Otherwise we need to first fetch the configuration.
  current_state_ = STATE_WAITING_FOR_PROXY_CONFIG;

  // Retrieve the current proxy configuration from the ProxyConfigService.
  // If a configuration is not available yet, we will get called back later
  // by our ProxyConfigService::Observer once it changes.
  ProxyConfigWithAnnotation config;
  ProxyConfigService::ConfigAvailability availability =
      config_service_->GetLatestProxyConfig(&config);
  if (availability != ProxyConfigService::CONFIG_PENDING)
    OnProxyConfigChanged(config, availability);
}

void ConfiguredProxyResolutionService::OnInitProxyResolverComplete(int result) {
  DCHECK_EQ(STATE_WAITING_FOR_INIT_PROXY_RESOLVER, current_state_);
  DCHECK(init_proxy_resolver_.get());
  DCHECK(fetched_config_);
  DCHECK(fetched_config_->value().HasAutomaticSettings());
  config_ = init_proxy_resolver_->effective_config();

  // At this point we have decided which proxy settings to use (i.e. which PAC
  // script if any). We start up a background poller to periodically revisit
  // this decision. If the contents of the PAC script change, or if the
  // result of proxy auto-discovery changes, this poller will notice it and
  // will trigger a re-initialization using the newly discovered PAC.
  script_poller_ = std::make_unique<PacFileDeciderPoller>(
      base::BindRepeating(
          &ConfiguredProxyResolutionService::InitializeUsingDecidedConfig,
          base::Unretained(this)),
      fetched_config_.value(), resolver_factory_->expects_pac_bytes(),
      pac_file_fetcher_.get(), dhcp_pac_file_fetcher_.get(), result,
      init_proxy_resolver_->script_data(), net_log_);
  script_poller_->set_quick_check_enabled(quick_check_enabled_);

  init_proxy_resolver_.reset();

  if (result != OK) {
    if (fetched_config_->value().pac_mandatory()) {
      VLOG(1) << "Failed configuring with mandatory PAC script, blocking all "
                 "traffic.";
      config_ = fetched_config_;
      result = ERR_MANDATORY_PROXY_CONFIGURATION_FAILED;
    } else {
      VLOG(1) << "Failed configuring with PAC script, falling-back to manual "
                 "proxy servers.";
      ProxyConfig proxy_config = fetched_config_->value();
      proxy_config.ClearAutomaticSettings();
      config_ = ProxyConfigWithAnnotation(
          proxy_config, fetched_config_->traffic_annotation());
      result = OK;
    }
  }
  permanent_error_ = result;

  // Resume any requests which we had to defer until the PAC script was
  // downloaded.
  SetReady();
}

void ConfiguredProxyResolutionService::ReportSuccess(const ProxyInfo& result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const ProxyRetryInfoMap& new_retry_info = result.proxy_retry_info();
  if (new_retry_info.empty())
    return;

  if (proxy_delegate_) {
    proxy_delegate_->OnSuccessfulRequestAfterFailures(new_retry_info);
  }

  for (const auto& iter : new_retry_info) {
    auto existing = proxy_retry_info_.find(iter.first);
    if (existing == proxy_retry_info_.end()) {
      proxy_retry_info_[iter.first] = iter.second;
      if (proxy_delegate_) {
        const ProxyChain& bad_proxy = iter.first;
        DCHECK(!bad_proxy.is_direct());
        const ProxyRetryInfo& proxy_retry_info = iter.second;
        proxy_delegate_->OnFallback(bad_proxy, proxy_retry_info.net_error);
      }
    } else if (existing->second.bad_until < iter.second.bad_until) {
      existing->second.bad_until = iter.second.bad_until;
    }
  }
  if (net_log_) {
    net_log_->AddGlobalEntry(NetLogEventType::BAD_PROXY_LIST_REPORTED, [&] {
      return NetLogBadProxyListParams(&new_retry_info);
    });
  }
}

bool ConfiguredProxyResolutionService::ContainsPendingRequest(
    ConfiguredProxyResolutionRequest* req) {
  return pending_requests_.count(req) == 1;
}

void ConfiguredProxyResolutionService::RemovePendingRequest(
    ConfiguredProxyResolutionRequest* req) {
  DCHECK(ContainsPendingRequest(req));
  pending_requests_.erase(req);
}

int ConfiguredProxyResolutionService::DidFinishResolvingProxy(
    const GURL& url,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& method,
    ProxyInfo* result,
    int result_code,
    const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Log the result of the proxy resolution.
  if (result_code == OK) {
    // Allow the proxy delegate to interpose on the resolution decision,
    // possibly modifying the ProxyInfo.
    if (proxy_delegate_)
      proxy_delegate_->OnResolveProxy(url, network_anonymization_key, method,
                                      proxy_retry_info_, result);

    net_log.AddEvent(
        NetLogEventType::PROXY_RESOLUTION_SERVICE_RESOLVED_PROXY_LIST,
        [&] { return NetLogFinishedResolvingProxyParams(result); });

    // This check is done to only log the NetLog event when necessary, it's
    // not a performance optimization.
    if (!proxy_retry_info_.empty()) {
      result->DeprioritizeBadProxyChains(proxy_retry_info_);
      net_log.AddEvent(
          NetLogEventType::PROXY_RESOLUTION_SERVICE_DEPRIORITIZED_BAD_PROXIES,
          [&] { return NetLogFinishedResolvingProxyParams(result); });
    }
  } else {
    net_log.AddEventWithNetErrorCode(
        NetLogEventType::PROXY_RESOLUTION_SERVICE_RESOLVED_PROXY_LIST,
        result_code);

    bool reset_config = result_code == ERR_PAC_SCRIPT_TERMINATED;
    if (config_ && !config_->value().pac_mandatory()) {
      // Fall-back to direct when the proxy resolver fails. This corresponds
      // with a javascript runtime error in the PAC script.
      //
      // This implicit fall-back to direct matches Firefox 3.5 and
      // Internet Explorer 8. For more information, see:
      //
      // http://www.chromium.org/developers/design-documents/proxy-settings-fallback
      result->UseDirect();
      result_code = OK;

      // Allow the proxy delegate to interpose on the resolution decision,
      // possibly modifying the ProxyInfo.
      if (proxy_delegate_)
        proxy_delegate_->OnResolveProxy(url, network_anonymization_key, method,
                                        proxy_retry_info_, result);
    } else {
      result_code = ERR_MANDATORY_PROXY_CONFIGURATION_FAILED;
    }
    if (reset_config) {
      ResetProxyConfig(false);
      // If the ProxyResolver crashed, force it to be re-initialized for the
      // next request by resetting the proxy config. If there are other pending
      // requests, trigger the recreation immediately so those requests retry.
      if (pending_requests_.size() > 1)
        ApplyProxyConfigIfAvailable();
    }
  }

  net_log.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  return result_code;
}

void ConfiguredProxyResolutionService::SetPacFileFetchers(
    std::unique_ptr<PacFileFetcher> pac_file_fetcher,
    std::unique_ptr<DhcpPacFileFetcher> dhcp_pac_file_fetcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  State previous_state = ResetProxyConfig(false);
  pac_file_fetcher_ = std::move(pac_file_fetcher);
  dhcp_pac_file_fetcher_ = std::move(dhcp_pac_file_fetcher);
  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

void ConfiguredProxyResolutionService::SetProxyDelegate(
    ProxyDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!proxy_delegate_ || !delegate);
  proxy_delegate_ = delegate;
}

void ConfiguredProxyResolutionService::OnShutdown() {
  // Order here does not matter for correctness. |init_proxy_resolver_| is first
  // because shutting it down also cancels its requests using the fetcher.
  if (init_proxy_resolver_)
    init_proxy_resolver_->OnShutdown();
  if (pac_file_fetcher_)
    pac_file_fetcher_->OnShutdown();
  if (dhcp_pac_file_fetcher_)
    dhcp_pac_file_fetcher_->OnShutdown();
}

const ProxyRetryInfoMap& ConfiguredProxyResolutionService::proxy_retry_info()
    const {
  return proxy_retry_info_;
}

void ConfiguredProxyResolutionService::ClearBadProxiesCache() {
  proxy_retry_info_.clear();
}

PacFileFetcher* ConfiguredProxyResolutionService::GetPacFileFetcher() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return pac_file_fetcher_.get();
}

bool ConfiguredProxyResolutionService::GetLoadStateIfAvailable(
    LoadState* load_state) const {
  if (current_state_ == STATE_WAITING_FOR_INIT_PROXY_RESOLVER) {
    *load_state = init_proxy_resolver_->GetLoadState();
    return true;
  }

  return false;
}

ProxyResolver* ConfiguredProxyResolutionService::GetProxyResolver() const {
  return resolver_.get();
}

ConfiguredProxyResolutionService::State
ConfiguredProxyResolutionService::ResetProxyConfig(bool reset_fetched_config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  State previous_state = current_state_;

  permanent_error_ = OK;
  proxy_retry_info_.clear();
  script_poller_.reset();
  init_proxy_resolver_.reset();
  SuspendAllPendingRequests();
  resolver_.reset();
  config_ = std::nullopt;
  if (reset_fetched_config)
    fetched_config_ = std::nullopt;
  current_state_ = STATE_NONE;

  return previous_state;
}

void ConfiguredProxyResolutionService::ForceReloadProxyConfig() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResetProxyConfig(false);
  ApplyProxyConfigIfAvailable();
}

base::Value::Dict ConfiguredProxyResolutionService::GetProxyNetLogValues() {
  base::Value::Dict net_info_dict;

  // Log Proxy Settings.
  {
    base::Value::Dict dict;
    if (fetched_config_)
      dict.Set("original", fetched_config_->value().ToValue());
    if (config_)
      dict.Set("effective", config_->value().ToValue());

    net_info_dict.Set(kNetInfoProxySettings, std::move(dict));
  }

  // Log Bad Proxies.
  {
    base::Value::List list;

    for (const auto& it : proxy_retry_info_) {
      const std::string& proxy_chain_uri = it.first.ToDebugString();
      const ProxyRetryInfo& retry_info = it.second;

      base::Value::Dict dict;
      dict.Set("proxy_chain_uri", proxy_chain_uri);
      dict.Set("bad_until", NetLog::TickCountToString(retry_info.bad_until));

      list.Append(base::Value(std::move(dict)));
    }

    net_info_dict.Set(kNetInfoBadProxies, std::move(list));
  }

  return net_info_dict;
}

bool ConfiguredProxyResolutionService::CastToConfiguredProxyResolutionService(
    ConfiguredProxyResolutionService** configured_proxy_resolution_service) {
  *configured_proxy_resolution_service = this;
  return true;
}

// static
const ConfiguredProxyResolutionService::PacPollPolicy*
ConfiguredProxyResolutionService::set_pac_script_poll_policy(
    const PacPollPolicy* policy) {
  return PacFileDeciderPoller::set_policy(policy);
}

// static
std::unique_ptr<ConfiguredProxyResolutionService::PacPollPolicy>
ConfiguredProxyResolutionService::CreateDefaultPacPollPolicy() {
  return std::make_unique<DefaultPollPolicy>();
}

void ConfiguredProxyResolutionService::OnProxyConfigChanged(
    const ProxyConfigWithAnnotation& config,
    ProxyConfigService::ConfigAvailability availability) {
  // Retrieve the current proxy configuration from the ProxyConfigService.
  // If a configuration is not available yet, we will get called back later
  // by our ProxyConfigService::Observer once it changes.
  ProxyConfigWithAnnotation effective_config;
  switch (availability) {
    case ProxyConfigService::CONFIG_PENDING:
      // ProxyConfigService implementors should never pass CONFIG_PENDING.
      NOTREACHED_IN_MIGRATION()
          << "Proxy config change with CONFIG_PENDING availability!";
      return;
    case ProxyConfigService::CONFIG_VALID:
      effective_config = config;
      break;
    case ProxyConfigService::CONFIG_UNSET:
      effective_config = ProxyConfigWithAnnotation::CreateDirect();
      break;
  }

  // Emit the proxy settings change to the NetLog stream.
  if (net_log_) {
    net_log_->AddGlobalEntry(NetLogEventType::PROXY_CONFIG_CHANGED, [&] {
      return NetLogProxyConfigChangedParams(&fetched_config_,
                                            &effective_config);
    });
  }

  // Set the new configuration as the most recently fetched one.
  fetched_config_ = effective_config;

  InitializeUsingLastFetchedConfig();
}

bool ConfiguredProxyResolutionService::ApplyPacBypassRules(const GURL& url,
                                                           ProxyInfo* results) {
  DCHECK(config_);

  if (ProxyBypassRules::MatchesImplicitRules(url)) {
    results->UseDirectWithBypassedProxy();
    return true;
  }

  return false;
}

void ConfiguredProxyResolutionService::InitializeUsingLastFetchedConfig() {
  ResetProxyConfig(false);

  DCHECK(fetched_config_);
  if (!fetched_config_->value().HasAutomaticSettings()) {
    config_ = fetched_config_;
    SetReady();
    return;
  }

  // Start downloading + testing the PAC scripts for this new configuration.
  current_state_ = STATE_WAITING_FOR_INIT_PROXY_RESOLVER;

  // If we changed networks recently, we should delay running proxy auto-config.
  base::TimeDelta wait_delay = stall_proxy_autoconfig_until_ - TimeTicks::Now();

  init_proxy_resolver_ = std::make_unique<InitProxyResolver>();
  init_proxy_resolver_->set_quick_check_enabled(quick_check_enabled_);
  int rv = init_proxy_resolver_->Start(
      &resolver_, resolver_factory_.get(), pac_file_fetcher_.get(),
      dhcp_pac_file_fetcher_.get(), net_log_, fetched_config_.value(),
      wait_delay,
      base::BindOnce(
          &ConfiguredProxyResolutionService::OnInitProxyResolverComplete,
          base::Unretained(this)));

  if (rv != ERR_IO_PENDING)
    OnInitProxyResolverComplete(rv);
}

void ConfiguredProxyResolutionService::InitializeUsingDecidedConfig(
    int decider_result,
    const PacFileDataWithSource& script_data,
    const ProxyConfigWithAnnotation& effective_config) {
  DCHECK(fetched_config_);
  DCHECK(fetched_config_->value().HasAutomaticSettings());

  ResetProxyConfig(false);

  current_state_ = STATE_WAITING_FOR_INIT_PROXY_RESOLVER;

  init_proxy_resolver_ = std::make_unique<InitProxyResolver>();
  int rv = init_proxy_resolver_->StartSkipDecider(
      &resolver_, resolver_factory_.get(), effective_config, decider_result,
      script_data,
      base::BindOnce(
          &ConfiguredProxyResolutionService::OnInitProxyResolverComplete,
          base::Unretained(this)));

  if (rv != ERR_IO_PENDING)
    OnInitProxyResolverComplete(rv);
}

void ConfiguredProxyResolutionService::OnIPAddressChanged() {
  // See the comment block by |kDelayAfterNetworkChangesMs| for info.
  stall_proxy_autoconfig_until_ =
      TimeTicks::Now() + stall_proxy_auto_config_delay_;

  // With a new network connection, using the proper proxy configuration for the
  // new connection may be essential for URL requests to work properly. Reset
  // the config to ensure new URL requests are blocked until the potential new
  // proxy configuration is loaded.
  State previous_state = ResetProxyConfig(false);
  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

void ConfiguredProxyResolutionService::OnDNSChanged() {
  // Do not fully reset proxy config on DNS change notifications. Instead,
  // inform the poller that it would be a good time to check for changes.
  //
  // While a change to DNS servers in use could lead to different WPAD results,
  // and thus a different proxy configuration, it is extremely unlikely to ever
  // be essential for that changed proxy configuration to be picked up
  // immediately. Either URL requests on the connection are generally working
  // fine without the proxy, or requests are already broken, leaving little harm
  // in letting a couple more requests fail until Chrome picks up the new proxy.
  if (script_poller_.get())
    script_poller_->OnLazyPoll();
}

}  // namespace net
