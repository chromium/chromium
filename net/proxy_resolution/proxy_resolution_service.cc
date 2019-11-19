// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_resolution_service.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_delegate.h"
#include "net/base/url_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"
#include "net/proxy_resolution/multi_threaded_proxy_resolver.h"
#include "net/proxy_resolution/pac_file_decider.h"
#include "net/proxy_resolution/pac_file_fetcher.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "net/proxy_resolution/proxy_config_service_win.h"
#include "net/proxy_resolution/proxy_resolver_winhttp.h"
#elif defined(OS_IOS)
#include "net/proxy_resolution/proxy_config_service_ios.h"
#include "net/proxy_resolution/proxy_resolver_mac.h"
#elif defined(OS_MACOSX)
#include "net/proxy_resolution/proxy_config_service_mac.h"
#include "net/proxy_resolution/proxy_resolver_mac.h"
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "net/proxy_resolution/proxy_config_service_linux.h"
#elif defined(OS_ANDROID)
#include "net/proxy_resolution/proxy_config_service_android.h"
#endif

using base::TimeDelta;
using base::TimeTicks;

namespace net {

namespace {

#if defined(OS_WIN) || defined(OS_IOS) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
constexpr net::NetworkTrafficAnnotationTag kSystemProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("proxy_config_system", R"(
      semantics {
        sender: "Proxy Config"
        description:
          "Establishing a connection through a proxy server using system proxy "
          "settings."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, and they indicate to use a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other:
          "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "User cannot override system proxy settings, but can change them "
          "through 'Advanced/System/Open proxy settings'."
        policy_exception_justification:
          "Using either of 'ProxyMode', 'ProxyServer', or 'ProxyPacUrl' "
          "policies can set Chrome to use a specific proxy settings and avoid "
          "system proxy."
      })");
#endif

const size_t kDefaultNumPacThreads = 4;

// When the IP address changes we don't immediately re-run proxy auto-config.
// Instead, we  wait for |kDelayAfterNetworkChangesMs| before
// attempting to re-valuate proxy auto-config.
//
// During this time window, any resolve requests sent to the
// ProxyResolutionService will be queued. Once we have waited the required
// amount of them, the proxy auto-config step will be run, and the queued
// requests resumed.
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
class DefaultPollPolicy : public ProxyResolutionService::PacPollPolicy {
 public:
  DefaultPollPolicy() = default;

  Mode GetNextDelay(int initial_error,
                    TimeDelta current_delay,
                    TimeDelta* next_delay) const override {
    if (initial_error != OK) {
      // Re-try policy for failures.
      const int kDelay1Seconds = 8;
      const int kDelay2Seconds = 32;
      const int kDelay3Seconds = 2 * 60;  // 2 minutes
      const int kDelay4Seconds = 4 * 60 * 60;  // 4 Hours

      // Initial poll.
      if (current_delay < TimeDelta()) {
        *next_delay = TimeDelta::FromSeconds(kDelay1Seconds);
        return MODE_USE_TIMER;
      }
      switch (current_delay.InSeconds()) {
        case kDelay1Seconds:
          *next_delay = TimeDelta::FromSeconds(kDelay2Seconds);
          return MODE_START_AFTER_ACTIVITY;
        case kDelay2Seconds:
          *next_delay = TimeDelta::FromSeconds(kDelay3Seconds);
          return MODE_START_AFTER_ACTIVITY;
        default:
          *next_delay = TimeDelta::FromSeconds(kDelay4Seconds);
          return MODE_START_AFTER_ACTIVITY;
      }
    } else {
      // Re-try policy for succeses.
      *next_delay = TimeDelta::FromHours(12);
      return MODE_START_AFTER_ACTIVITY;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPollPolicy);
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
                     const NetworkIsolationKey& network_isolation_key,
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
                     const NetworkIsolationKey& network_isolation_key,
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

// Creates ProxyResolvers using a platform-specific implementation.
class ProxyResolverFactoryForSystem : public MultiThreadedProxyResolverFactory {
 public:
  explicit ProxyResolverFactoryForSystem(size_t max_num_threads)
      : MultiThreadedProxyResolverFactory(max_num_threads,
                                          false /*expects_pac_bytes*/) {}

  std::unique_ptr<ProxyResolverFactory> CreateProxyResolverFactory() override {
#if defined(OS_WIN)
    return std::make_unique<ProxyResolverFactoryWinHttp>();
#elif defined(OS_MACOSX)
    return std::make_unique<ProxyResolverFactoryMac>();
#else
    NOTREACHED();
    return NULL;
#endif
  }

  static bool IsSupported() {
#if defined(OS_WIN) || defined(OS_MACOSX)
    return true;
#else
    return false;
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryForSystem);
};

class ProxyResolverFactoryForNullResolver : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryForNullResolver() : ProxyResolverFactory(false) {}

  // ProxyResolverFactory overrides.
  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override {
    resolver->reset(new ProxyResolverNull());
    return OK;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryForNullResolver);
};

class ProxyResolverFactoryForPacResult : public ProxyResolverFactory {
 public:
  explicit ProxyResolverFactoryForPacResult(const std::string& pac_string)
      : ProxyResolverFactory(false), pac_string_(pac_string) {}

  // ProxyResolverFactory override.
  int CreateProxyResolver(const scoped_refptr<PacFileData>& pac_script,
                          std::unique_ptr<ProxyResolver>* resolver,
                          CompletionOnceCallback callback,
                          std::unique_ptr<Request>* request) override {
    resolver->reset(new ProxyResolverFromPacString(pac_string_));
    return OK;
  }

 private:
  const std::string pac_string_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverFactoryForPacResult);
};

// Returns NetLog parameters describing a proxy configuration change.
base::Value NetLogProxyConfigChangedParams(
    const base::Optional<ProxyConfigWithAnnotation>* old_config,
    const ProxyConfigWithAnnotation* new_config) {
  base::Value dict(base::Value::Type::DICTIONARY);
  // The "old_config" is optional -- the first notification will not have
  // any "previous" configuration.
  if (old_config->has_value())
    dict.SetKey("old_config", (*old_config)->value().ToValue());
  dict.SetKey("new_config", new_config->value().ToValue());
  return dict;
}

base::Value NetLogBadProxyListParams(const ProxyRetryInfoMap* retry_info) {
  base::Value dict(base::Value::Type::DICTIONARY);
  base::Value list(base::Value::Type::LIST);

  for (const auto& retry_info_pair : *retry_info)
    list.Append(retry_info_pair.first);
  dict.SetKey("bad_proxy_list", std::move(list));
  return dict;
}

// Returns NetLog parameters on a successfuly proxy resolution.
base::Value NetLogFinishedResolvingProxyParams(const ProxyInfo* result) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("pac_string", result->ToPacString());
  return dict;
}

#if defined(OS_CHROMEOS)
class UnsetProxyConfigService : public ProxyConfigService {
 public:
  UnsetProxyConfigService() = default;
  ~UnsetProxyConfigService() override = default;

  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  ConfigAvailability GetLatestProxyConfig(
      ProxyConfigWithAnnotation* config) override {
    return CONFIG_UNSET;
  }
};
#endif

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
// TODO(https://crbug.com/882536): Use the same stripping for insecure URL
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

// Do not change the enumerated value as it is relied on by histograms.
enum class PacUrlSchemeForHistogram {
  kOther = 0,

  kHttp = 1,
  kHttps = 2,
  kFtp = 3,
  kFile = 4,
  kData = 5,

  kMaxValue = kData,
};

PacUrlSchemeForHistogram GetPacUrlScheme(const GURL& pac_url) {
  if (pac_url.SchemeIs("http"))
    return PacUrlSchemeForHistogram::kHttp;
  if (pac_url.SchemeIs("https"))
    return PacUrlSchemeForHistogram::kHttps;
  if (pac_url.SchemeIs("data"))
    return PacUrlSchemeForHistogram::kData;
  if (pac_url.SchemeIs("ftp"))
    return PacUrlSchemeForHistogram::kFtp;
  if (pac_url.SchemeIs("file"))
    return PacUrlSchemeForHistogram::kFile;

  return PacUrlSchemeForHistogram::kOther;
}

}  // namespace

// ProxyResolutionService::InitProxyResolver ----------------------------------

// This glues together two asynchronous steps:
//   (1) PacFileDecider -- try to fetch/validate a sequence of PAC scripts
//       to figure out what we should configure against.
//   (2) Feed the fetched PAC script into the ProxyResolver.
//
// InitProxyResolver is a single-use class which encapsulates cancellation as
// part of its destructor. Start() or StartSkipDecider() should be called just
// once. The instance can be destroyed at any time, and the request will be
// cancelled.

class ProxyResolutionService::InitProxyResolver {
 public:
  InitProxyResolver()
      : proxy_resolver_factory_(nullptr),
        proxy_resolver_(nullptr),
        next_state_(STATE_NONE),
        quick_check_enabled_(true) {}

  ~InitProxyResolver() {
    // Note that the destruction of PacFileDecider will automatically cancel
    // any outstanding work.
  }

  // Begins initializing the proxy resolver; calls |callback| when done. A
  // ProxyResolver instance will be created using |proxy_resolver_factory| and
  // assigned to |*proxy_resolver| if the final result is OK.
  int Start(std::unique_ptr<ProxyResolver>* proxy_resolver,
            ProxyResolverFactory* proxy_resolver_factory,
            PacFileFetcher* pac_file_fetcher,
            DhcpPacFileFetcher* dhcp_pac_file_fetcher,
            NetLog* net_log,
            const ProxyConfigWithAnnotation& config,
            TimeDelta wait_delay,
            CompletionOnceCallback callback) {
    DCHECK_EQ(STATE_NONE, next_state_);
    proxy_resolver_ = proxy_resolver;
    proxy_resolver_factory_ = proxy_resolver_factory;

    decider_.reset(
        new PacFileDecider(pac_file_fetcher, dhcp_pac_file_fetcher, net_log));
    decider_->set_quick_check_enabled(quick_check_enabled_);
    config_ = config;
    wait_delay_ = wait_delay;
    callback_ = std::move(callback);

    next_state_ = STATE_DECIDE_PAC_FILE;
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
    DCHECK_EQ(STATE_NONE, next_state_);
    proxy_resolver_ = proxy_resolver;
    proxy_resolver_factory_ = proxy_resolver_factory;

    effective_config_ = effective_config;
    script_data_ = script_data;
    callback_ = std::move(callback);

    if (decider_result != OK)
      return decider_result;

    next_state_ = STATE_CREATE_RESOLVER;
    return DoLoop(OK);
  }

  // Returns the proxy configuration that was selected by PacFileDecider.
  // Should only be called upon completion of the initialization.
  const ProxyConfigWithAnnotation& effective_config() const {
    DCHECK_EQ(STATE_NONE, next_state_);
    return effective_config_;
  }

  // Returns the PAC script data that was selected by PacFileDecider.
  // Should only be called upon completion of the initialization.
  const PacFileDataWithSource& script_data() {
    DCHECK_EQ(STATE_NONE, next_state_);
    return script_data_;
  }

  LoadState GetLoadState() const {
    if (next_state_ == STATE_DECIDE_PAC_FILE_COMPLETE) {
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
  enum State {
    STATE_NONE,
    STATE_DECIDE_PAC_FILE,
    STATE_DECIDE_PAC_FILE_COMPLETE,
    STATE_CREATE_RESOLVER,
    STATE_CREATE_RESOLVER_COMPLETE,
  };

  int DoLoop(int result) {
    DCHECK_NE(next_state_, STATE_NONE);
    int rv = result;
    do {
      State state = next_state_;
      next_state_ = STATE_NONE;
      switch (state) {
        case STATE_DECIDE_PAC_FILE:
          DCHECK_EQ(OK, rv);
          rv = DoDecidePacFile();
          break;
        case STATE_DECIDE_PAC_FILE_COMPLETE:
          rv = DoDecidePacFileComplete(rv);
          break;
        case STATE_CREATE_RESOLVER:
          DCHECK_EQ(OK, rv);
          rv = DoCreateResolver();
          break;
        case STATE_CREATE_RESOLVER_COMPLETE:
          rv = DoCreateResolverComplete(rv);
          break;
        default:
          NOTREACHED() << "bad state: " << state;
          rv = ERR_UNEXPECTED;
          break;
      }
    } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
    return rv;
  }

  int DoDecidePacFile() {
    next_state_ = STATE_DECIDE_PAC_FILE_COMPLETE;

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

    next_state_ = STATE_CREATE_RESOLVER;
    return OK;
  }

  int DoCreateResolver() {
    DCHECK(script_data_.data);
    // TODO(eroman): Should log this latency to the NetLog.
    next_state_ = STATE_CREATE_RESOLVER_COMPLETE;
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
    DCHECK_NE(STATE_NONE, next_state_);
    int rv = DoLoop(result);
    if (rv != ERR_IO_PENDING)
      std::move(callback_).Run(result);
  }

  ProxyConfigWithAnnotation config_;
  ProxyConfigWithAnnotation effective_config_;
  PacFileDataWithSource script_data_;
  TimeDelta wait_delay_;
  std::unique_ptr<PacFileDecider> decider_;
  ProxyResolverFactory* proxy_resolver_factory_;
  std::unique_ptr<ProxyResolverFactory::Request> create_resolver_request_;
  std::unique_ptr<ProxyResolver>* proxy_resolver_;
  CompletionOnceCallback callback_;
  State next_state_;
  bool quick_check_enabled_;

  DISALLOW_COPY_AND_ASSIGN(InitProxyResolver);
};

// ProxyResolutionService::PacFileDeciderPoller ---------------------------

// This helper class encapsulates the logic to schedule and run periodic
// background checks to see if the PAC script (or effective proxy configuration)
// has changed. If a change is detected, then the caller will be notified via
// the ChangeCallback.
class ProxyResolutionService::PacFileDeciderPoller {
 public:
  typedef base::Callback<
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
        last_poll_time_(TimeTicks::Now()) {
    // Set the initial poll delay.
    next_poll_mode_ = poll_policy()->GetNextDelay(
        last_error_, TimeDelta::FromSeconds(-1), &next_poll_delay_);
    TryToStartNextPoll(false);
  }

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

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
          TimeDelta elapsed_time = TimeTicks::Now() - last_poll_time_;
          if (elapsed_time >= next_poll_delay_)
            DoPoll();
        }
        break;
    }
  }

  void DoPoll() {
    last_poll_time_ = TimeTicks::Now();

    // Start the PAC file decider to see if anything has changed.
    // TODO(eroman): Pass a proper NetLog rather than NULL.
    decider_.reset(
        new PacFileDecider(pac_file_fetcher_, dhcp_pac_file_fetcher_, nullptr));
    decider_->set_quick_check_enabled(quick_check_enabled_);
    int result = decider_->Start(
        config_, TimeDelta(), proxy_resolver_expects_pac_bytes_,
        base::BindOnce(&PacFileDeciderPoller::OnPacFileDeciderCompleted,
                       base::Unretained(this)));

    if (result != ERR_IO_PENDING)
      OnPacFileDeciderCompleted(result);
  }

  void OnPacFileDeciderCompleted(int result) {
    if (HasScriptDataChanged(result, decider_->script_data())) {
      // Something has changed, we must notify the ProxyResolutionService so it
      // can re-initialize its ProxyResolver. Note that we post a notification
      // task rather than calling it directly -- this is done to avoid an ugly
      // destruction sequence, since |this| might be destroyed as a result of
      // the notification.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
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
    next_poll_mode_ = poll_policy()->GetNextDelay(
        last_error_, next_poll_delay_, &next_poll_delay_);
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
    // ProxyResolutionService.
    change_callback_.Run(result, script_data, effective_config);
  }

  ChangeCallback change_callback_;
  ProxyConfigWithAnnotation config_;
  bool proxy_resolver_expects_pac_bytes_;
  PacFileFetcher* pac_file_fetcher_;
  DhcpPacFileFetcher* dhcp_pac_file_fetcher_;

  int last_error_;
  PacFileDataWithSource last_script_data_;

  std::unique_ptr<PacFileDecider> decider_;
  TimeDelta next_poll_delay_;
  PacPollPolicy::Mode next_poll_mode_;

  TimeTicks last_poll_time_;

  // Polling policy injected by unit-tests. Otherwise this is NULL and the
  // default policy will be used.
  static const PacPollPolicy* poll_policy_;

  const DefaultPollPolicy default_poll_policy_;

  bool quick_check_enabled_;

  base::WeakPtrFactory<PacFileDeciderPoller> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PacFileDeciderPoller);
};

// static
const ProxyResolutionService::PacPollPolicy*
    ProxyResolutionService::PacFileDeciderPoller::poll_policy_ = nullptr;

class ProxyResolutionService::RequestImpl
    : public ProxyResolutionService::Request {
 public:
  RequestImpl(ProxyResolutionService* service,
              const GURL& url,
              const std::string& method,
              const NetworkIsolationKey& network_isolation_key,
              ProxyInfo* results,
              const CompletionOnceCallback user_callback,
              const NetLogWithSource& net_log);
  ~RequestImpl() override;

  // Starts the resolve proxy request.
  int Start();

  bool is_started() const {
    // Note that !! casts to bool. (VS gives a warning otherwise).
    return !!resolve_job_.get();
  }

  void StartAndCompleteCheckingForSynchronous();

  void CancelResolveJob();

  // Returns true if the request has been completed.
  bool was_completed() const { return user_callback_.is_null(); }

  // Callback for when the ProxyResolver request has completed.
  void QueryComplete(int result_code);

  // Helper to call after ProxyResolver completion (both synchronous and
  // asynchronous). Fixes up the result that is to be returned to user.
  int QueryDidComplete(int result_code);

  // Helper to call if the request completes synchronously, since in that case
  // the request will not be added to |pending_requests_| (in
  // |ProxyResolutionService|).
  int QueryDidCompleteSynchronously(int result_code);

  NetLogWithSource* net_log() { return &net_log_; }

  // Request implementation:
  LoadState GetLoadState() const override;

 private:
  ProxyResolver* resolver() const { return service_->resolver_.get(); }

  // Note that Request holds a bare pointer to the ProxyResolutionService.
  // Outstanding requests are cancelled during ~ProxyResolutionService, so this
  // is guaranteed to be valid throughout our lifetime.
  ProxyResolutionService* service_;
  CompletionOnceCallback user_callback_;
  ProxyInfo* results_;
  const GURL url_;
  const std::string method_;
  const NetworkIsolationKey network_isolation_key_;
  std::unique_ptr<ProxyResolver::Request> resolve_job_;
  MutableNetworkTrafficAnnotationTag traffic_annotation_;
  NetLogWithSource net_log_;
  // Time when the request was created.  Stored here rather than in |results_|
  // because the time in |results_| will be cleared.
  base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

ProxyResolutionService::RequestImpl::RequestImpl(
    ProxyResolutionService* service,
    const GURL& url,
    const std::string& method,
    const NetworkIsolationKey& network_isolation_key,
    ProxyInfo* results,
    CompletionOnceCallback user_callback,
    const NetLogWithSource& net_log)
    : service_(service),
      user_callback_(std::move(user_callback)),
      results_(results),
      url_(url),
      method_(method),
      network_isolation_key_(network_isolation_key),
      resolve_job_(nullptr),
      net_log_(net_log),
      creation_time_(base::TimeTicks::Now()) {
  DCHECK(!user_callback_.is_null());
}

ProxyResolutionService::RequestImpl::~RequestImpl() {
  if (service_) {
    service_->RemovePendingRequest(this);
    net_log_.AddEvent(NetLogEventType::CANCELLED);

    if (is_started())
      CancelResolveJob();

    // This should be emitted last, after any message |CancelResolveJob()| may
    // trigger.
    net_log_.EndEvent(NetLogEventType::PROXY_RESOLUTION_SERVICE);
  }
}

// Starts the resolve proxy request.
int ProxyResolutionService::RequestImpl::Start() {
  DCHECK(!was_completed());
  DCHECK(!is_started());

  DCHECK(service_->config_);
  traffic_annotation_ = MutableNetworkTrafficAnnotationTag(
      service_->config_->traffic_annotation());

  if (service_->ApplyPacBypassRules(url_, results_))
    return OK;

  return resolver()->GetProxyForURL(
      url_, network_isolation_key_, results_,
      base::BindOnce(&ProxyResolutionService::RequestImpl::QueryComplete,
                     base::Unretained(this)),
      &resolve_job_, net_log_);
}

void ProxyResolutionService::RequestImpl::
    StartAndCompleteCheckingForSynchronous() {
  int rv = service_->TryToCompleteSynchronously(url_, results_);
  if (rv == ERR_IO_PENDING)
    rv = Start();
  if (rv != ERR_IO_PENDING)
    QueryComplete(rv);
}

void ProxyResolutionService::RequestImpl::CancelResolveJob() {
  DCHECK(is_started());
  // The request may already be running in the resolver.
  resolve_job_.reset();
  DCHECK(!is_started());
}

int ProxyResolutionService::RequestImpl::QueryDidComplete(int result_code) {
  DCHECK(!was_completed());

  // Clear |resolve_job_| so is_started() returns false while
  // DidFinishResolvingProxy() runs.
  resolve_job_.reset();

  // Note that DidFinishResolvingProxy might modify |results_|.
  int rv = service_->DidFinishResolvingProxy(url_, method_, results_,
                                             result_code, net_log_);

  // Make a note in the results which configuration was in use at the
  // time of the resolve.
  results_->did_use_pac_script_ = true;
  results_->proxy_resolve_start_time_ = creation_time_;
  results_->proxy_resolve_end_time_ = TimeTicks::Now();

  // If annotation is not already set, e.g. through TryToCompleteSynchronously
  // function, use in-progress-resolve annotation.
  if (!results_->traffic_annotation_.is_valid())
    results_->set_traffic_annotation(traffic_annotation_);

  // If proxy is set without error, ensure that an annotation is provided.
  if (result_code != ERR_ABORTED && !rv)
    DCHECK(results_->traffic_annotation_.is_valid());

  // Reset the state associated with in-progress-resolve.
  traffic_annotation_.reset();

  return rv;
}

int ProxyResolutionService::RequestImpl::QueryDidCompleteSynchronously(
    int result_code) {
  int rv = QueryDidComplete(result_code);
  service_ = nullptr;
  return rv;
}

LoadState ProxyResolutionService::RequestImpl::GetLoadState() const {
  if (service_ &&
      service_->current_state_ == STATE_WAITING_FOR_INIT_PROXY_RESOLVER) {
    return service_->init_proxy_resolver_->GetLoadState();
  }

  if (is_started())
    return resolve_job_->GetLoadState();
  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

// Callback for when the ProxyResolver request has completed.
void ProxyResolutionService::RequestImpl::QueryComplete(int result_code) {
  result_code = QueryDidComplete(result_code);

  CompletionOnceCallback callback = std::move(user_callback_);

  service_->RemovePendingRequest(this);
  service_ = nullptr;
  user_callback_.Reset();
  std::move(callback).Run(result_code);
}

// ProxyResolutionService -----------------------------------------------------

ProxyResolutionService::ProxyResolutionService(
    std::unique_ptr<ProxyConfigService> config_service,
    std::unique_ptr<ProxyResolverFactory> resolver_factory,
    NetLog* net_log)
    : config_service_(std::move(config_service)),
      resolver_factory_(std::move(resolver_factory)),
      current_state_(STATE_NONE),
      permanent_error_(OK),
      net_log_(net_log),
      stall_proxy_auto_config_delay_(
          TimeDelta::FromMilliseconds(kDelayAfterNetworkChangesMs)),
      quick_check_enabled_(true) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddDNSObserver(this);
  config_service_->AddObserver(this);
}

// static
std::unique_ptr<ProxyResolutionService>
ProxyResolutionService::CreateUsingSystemProxyResolver(
    std::unique_ptr<ProxyConfigService> proxy_config_service,
    NetLog* net_log) {
  DCHECK(proxy_config_service);

  if (!ProxyResolverFactoryForSystem::IsSupported()) {
    VLOG(1) << "PAC support disabled because there is no system implementation";
    return CreateWithoutProxyResolver(std::move(proxy_config_service), net_log);
  }

  return std::make_unique<ProxyResolutionService>(
      std::move(proxy_config_service),
      std::make_unique<ProxyResolverFactoryForSystem>(kDefaultNumPacThreads),
      net_log);
}

// static
std::unique_ptr<ProxyResolutionService>
ProxyResolutionService::CreateWithoutProxyResolver(
    std::unique_ptr<ProxyConfigService> proxy_config_service,
    NetLog* net_log) {
  return std::make_unique<ProxyResolutionService>(
      std::move(proxy_config_service),
      std::make_unique<ProxyResolverFactoryForNullResolver>(), net_log);
}

// static
std::unique_ptr<ProxyResolutionService> ProxyResolutionService::CreateFixed(
    const ProxyConfigWithAnnotation& pc) {
  // TODO(eroman): This isn't quite right, won't work if |pc| specifies
  //               a PAC script.
  return CreateUsingSystemProxyResolver(
      std::make_unique<ProxyConfigServiceFixed>(pc), nullptr);
}

// static
std::unique_ptr<ProxyResolutionService> ProxyResolutionService::CreateFixed(
    const std::string& proxy,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(proxy);
  ProxyConfigWithAnnotation annotated_config(proxy_config, traffic_annotation);
  return ProxyResolutionService::CreateFixed(annotated_config);
}

// static
std::unique_ptr<ProxyResolutionService> ProxyResolutionService::CreateDirect() {
  // Use direct connections.
  return std::make_unique<ProxyResolutionService>(
      std::make_unique<ProxyConfigServiceDirect>(),
      std::make_unique<ProxyResolverFactoryForNullResolver>(), nullptr);
}

// static
std::unique_ptr<ProxyResolutionService>
ProxyResolutionService::CreateFixedFromPacResult(
    const std::string& pac_string,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  // We need the settings to contain an "automatic" setting, otherwise the
  // ProxyResolver dependency we give it will never be used.
  std::unique_ptr<ProxyConfigService> proxy_config_service(
      new ProxyConfigServiceFixed(ProxyConfigWithAnnotation(
          ProxyConfig::CreateFromCustomPacURL(
              GURL("https://my-pac-script.invalid/wpad.dat")),
          traffic_annotation)));

  return std::make_unique<ProxyResolutionService>(
      std::move(proxy_config_service),
      std::make_unique<ProxyResolverFactoryForPacResult>(pac_string), nullptr);
}

// static
std::unique_ptr<ProxyResolutionService>
ProxyResolutionService::CreateFixedFromAutoDetectedPacResult(
    const std::string& pac_string,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  std::unique_ptr<ProxyConfigService> proxy_config_service(
      new ProxyConfigServiceFixed(ProxyConfigWithAnnotation(
          ProxyConfig::CreateAutoDetect(), traffic_annotation)));

  return std::make_unique<ProxyResolutionService>(
      std::move(proxy_config_service),
      std::make_unique<ProxyResolverFactoryForPacResult>(pac_string), nullptr);
}

int ProxyResolutionService::ResolveProxy(
    const GURL& raw_url,
    const std::string& method,
    const NetworkIsolationKey& network_isolation_key,
    ProxyInfo* result,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* out_request,
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
    rv = DidFinishResolvingProxy(url, method, result, rv, net_log);
    return rv;
  }

  std::unique_ptr<RequestImpl> req =
      std::make_unique<RequestImpl>(this, url, method, network_isolation_key,
                                    result, std::move(callback), net_log);

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

int ProxyResolutionService::TryToCompleteSynchronously(
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

ProxyResolutionService::~ProxyResolutionService() {
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
    RequestImpl* req = *pending_requests_.begin();
    req->QueryComplete(ERR_ABORTED);
    pending_requests_.erase(req);
  }
}

void ProxyResolutionService::SuspendAllPendingRequests() {
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();
       ++it) {
    RequestImpl* req = *it;
    if (req->is_started()) {
      req->CancelResolveJob();

      req->net_log()->BeginEvent(
          NetLogEventType::PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC);
    }
  }
}

void ProxyResolutionService::SetReady() {
  DCHECK(!init_proxy_resolver_.get());
  current_state_ = STATE_READY;

  // TODO(lilyhoughton): This is necessary because a callback invoked by
  // |StartAndCompleteCheckingForSynchronous()| might delete |this|.  A better
  // solution would be to disallow synchronous callbacks altogether.
  base::WeakPtr<ProxyResolutionService> weak_this =
      weak_ptr_factory_.GetWeakPtr();

  auto pending_requests_copy = pending_requests_;
  for (auto* req : pending_requests_copy) {
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

void ProxyResolutionService::ApplyProxyConfigIfAvailable() {
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

void ProxyResolutionService::OnInitProxyResolverComplete(int result) {
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
  script_poller_.reset(new PacFileDeciderPoller(
      base::Bind(&ProxyResolutionService::InitializeUsingDecidedConfig,
                 base::Unretained(this)),
      fetched_config_.value(), resolver_factory_->expects_pac_bytes(),
      pac_file_fetcher_.get(), dhcp_pac_file_fetcher_.get(), result,
      init_proxy_resolver_->script_data(), nullptr));
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

bool ProxyResolutionService::MarkProxiesAsBadUntil(
    const ProxyInfo& result,
    base::TimeDelta retry_delay,
    const std::vector<ProxyServer>& additional_bad_proxies,
    const NetLogWithSource& net_log) {
  result.proxy_list_.UpdateRetryInfoOnFallback(&proxy_retry_info_, retry_delay,
                                               false, additional_bad_proxies,
                                               OK, net_log);
  return result.proxy_list_.size() > (additional_bad_proxies.size() + 1);
}

void ProxyResolutionService::ReportSuccess(const ProxyInfo& result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const ProxyRetryInfoMap& new_retry_info = result.proxy_retry_info();
  if (new_retry_info.empty())
    return;

  for (auto iter = new_retry_info.begin(); iter != new_retry_info.end();
       ++iter) {
    auto existing = proxy_retry_info_.find(iter->first);
    if (existing == proxy_retry_info_.end()) {
      proxy_retry_info_[iter->first] = iter->second;
      if (proxy_delegate_) {
        const ProxyServer& bad_proxy =
            ProxyServer::FromURI(iter->first, ProxyServer::SCHEME_HTTP);
        const ProxyRetryInfo& proxy_retry_info = iter->second;
        proxy_delegate_->OnFallback(bad_proxy, proxy_retry_info.net_error);
      }
    }
    else if (existing->second.bad_until < iter->second.bad_until)
      existing->second.bad_until = iter->second.bad_until;
  }
  if (net_log_) {
    net_log_->AddGlobalEntry(NetLogEventType::BAD_PROXY_LIST_REPORTED, [&] {
      return NetLogBadProxyListParams(&new_retry_info);
    });
  }
}

bool ProxyResolutionService::ContainsPendingRequest(RequestImpl* req) {
  return pending_requests_.count(req) == 1;
}

void ProxyResolutionService::RemovePendingRequest(RequestImpl* req) {
  DCHECK(ContainsPendingRequest(req));
  pending_requests_.erase(req);
}

int ProxyResolutionService::DidFinishResolvingProxy(
    const GURL& url,
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
      proxy_delegate_->OnResolveProxy(url, method, proxy_retry_info_, result);

    net_log.AddEvent(
        NetLogEventType::PROXY_RESOLUTION_SERVICE_RESOLVED_PROXY_LIST,
        [&] { return NetLogFinishedResolvingProxyParams(result); });

    // This check is done to only log the NetLog event when necessary, it's
    // not a performance optimization.
    if (!proxy_retry_info_.empty()) {
      result->DeprioritizeBadProxies(proxy_retry_info_);
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
        proxy_delegate_->OnResolveProxy(url, method, proxy_retry_info_, result);
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

void ProxyResolutionService::SetPacFileFetchers(
    std::unique_ptr<PacFileFetcher> pac_file_fetcher,
    std::unique_ptr<DhcpPacFileFetcher> dhcp_pac_file_fetcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  State previous_state = ResetProxyConfig(false);
  pac_file_fetcher_ = std::move(pac_file_fetcher);
  dhcp_pac_file_fetcher_ = std::move(dhcp_pac_file_fetcher);
  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

void ProxyResolutionService::SetProxyDelegate(ProxyDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  proxy_delegate_ = delegate;
}

void ProxyResolutionService::AssertNoProxyDelegate() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!proxy_delegate_);
}

void ProxyResolutionService::OnShutdown() {
  // Order here does not matter for correctness. |init_proxy_resolver_| is first
  // because shutting it down also cancels its requests using the fetcher.
  if (init_proxy_resolver_)
    init_proxy_resolver_->OnShutdown();
  if (pac_file_fetcher_)
    pac_file_fetcher_->OnShutdown();
  if (dhcp_pac_file_fetcher_)
    dhcp_pac_file_fetcher_->OnShutdown();
}

PacFileFetcher* ProxyResolutionService::GetPacFileFetcher() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return pac_file_fetcher_.get();
}

ProxyResolutionService::State ProxyResolutionService::ResetProxyConfig(
    bool reset_fetched_config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  State previous_state = current_state_;

  permanent_error_ = OK;
  proxy_retry_info_.clear();
  script_poller_.reset();
  init_proxy_resolver_.reset();
  SuspendAllPendingRequests();
  resolver_.reset();
  config_ = base::nullopt;
  if (reset_fetched_config)
    fetched_config_ = base::nullopt;
  current_state_ = STATE_NONE;

  return previous_state;
}

void ProxyResolutionService::ForceReloadProxyConfig() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResetProxyConfig(false);
  ApplyProxyConfigIfAvailable();
}

// static
std::unique_ptr<ProxyConfigService>
ProxyResolutionService::CreateSystemProxyConfigService(
    const scoped_refptr<base::SequencedTaskRunner>& main_task_runner) {
#if defined(OS_WIN)
  return std::make_unique<ProxyConfigServiceWin>(
      kSystemProxyConfigTrafficAnnotation);
#elif defined(OS_IOS)
  return std::make_unique<ProxyConfigServiceIOS>(
      kSystemProxyConfigTrafficAnnotation);
#elif defined(OS_MACOSX)
  return std::make_unique<ProxyConfigServiceMac>(
      main_task_runner, kSystemProxyConfigTrafficAnnotation);
#elif defined(OS_CHROMEOS)
  LOG(ERROR) << "ProxyConfigService for ChromeOS should be created in "
             << "profile_io_data.cc::CreateProxyConfigService and this should "
             << "be used only for examples.";
  return std::make_unique<UnsetProxyConfigService>();
#elif defined(OS_LINUX)
  std::unique_ptr<ProxyConfigServiceLinux> linux_config_service(
      new ProxyConfigServiceLinux());

  // Assume we got called on the thread that runs the default glib
  // main loop, so the current thread is where we should be running
  // gsettings calls from.
  scoped_refptr<base::SingleThreadTaskRunner> glib_thread_task_runner =
      base::ThreadTaskRunnerHandle::Get();

  // Synchronously fetch the current proxy config (since we are running on
  // glib_default_loop). Additionally register for notifications (delivered in
  // either |glib_default_loop| or an internal sequenced task runner) to
  // keep us updated when the proxy config changes.
  linux_config_service->SetupAndFetchInitialConfig(
      glib_thread_task_runner, main_task_runner,
      kSystemProxyConfigTrafficAnnotation);

  return std::move(linux_config_service);
#elif defined(OS_ANDROID)
  return std::make_unique<ProxyConfigServiceAndroid>(
      main_task_runner, base::ThreadTaskRunnerHandle::Get());
#elif defined(OS_FUCHSIA)
  // TODO(crbug.com/889195): Implement a system proxy service for Fuchsia.
  return std::make_unique<ProxyConfigServiceDirect>();
#else
  LOG(WARNING) << "Failed to choose a system proxy settings fetcher "
                  "for this platform.";
  return std::make_unique<ProxyConfigServiceDirect>();
#endif
}

// static
const ProxyResolutionService::PacPollPolicy*
ProxyResolutionService::set_pac_script_poll_policy(
    const PacPollPolicy* policy) {
  return PacFileDeciderPoller::set_policy(policy);
}

// static
std::unique_ptr<ProxyResolutionService::PacPollPolicy>
ProxyResolutionService::CreateDefaultPacPollPolicy() {
  return std::unique_ptr<PacPollPolicy>(new DefaultPollPolicy());
}

void ProxyResolutionService::OnProxyConfigChanged(
    const ProxyConfigWithAnnotation& config,
    ProxyConfigService::ConfigAvailability availability) {
  // Retrieve the current proxy configuration from the ProxyConfigService.
  // If a configuration is not available yet, we will get called back later
  // by our ProxyConfigService::Observer once it changes.
  ProxyConfigWithAnnotation effective_config;
  switch (availability) {
    case ProxyConfigService::CONFIG_PENDING:
      // ProxyConfigService implementors should never pass CONFIG_PENDING.
      NOTREACHED() << "Proxy config change with CONFIG_PENDING availability!";
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

  if (config.value().has_pac_url()) {
    UMA_HISTOGRAM_ENUMERATION("Net.ProxyResolutionService.PacUrlScheme",
                              GetPacUrlScheme(config.value().pac_url()));
  }

  // Set the new configuration as the most recently fetched one.
  fetched_config_ = effective_config;

  InitializeUsingLastFetchedConfig();
}

bool ProxyResolutionService::ApplyPacBypassRules(const GURL& url,
                                                 ProxyInfo* results) {
  DCHECK(config_);

  if (ProxyBypassRules::MatchesImplicitRules(url)) {
    results->UseDirectWithBypassedProxy();
    return true;
  }

  return false;
}

void ProxyResolutionService::InitializeUsingLastFetchedConfig() {
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
  TimeDelta wait_delay =
      stall_proxy_autoconfig_until_ - TimeTicks::Now();

  init_proxy_resolver_.reset(new InitProxyResolver());
  init_proxy_resolver_->set_quick_check_enabled(quick_check_enabled_);
  int rv = init_proxy_resolver_->Start(
      &resolver_, resolver_factory_.get(), pac_file_fetcher_.get(),
      dhcp_pac_file_fetcher_.get(), net_log_, fetched_config_.value(),
      wait_delay,
      base::BindOnce(&ProxyResolutionService::OnInitProxyResolverComplete,
                     base::Unretained(this)));

  if (rv != ERR_IO_PENDING)
    OnInitProxyResolverComplete(rv);
}

void ProxyResolutionService::InitializeUsingDecidedConfig(
    int decider_result,
    const PacFileDataWithSource& script_data,
    const ProxyConfigWithAnnotation& effective_config) {
  DCHECK(fetched_config_);
  DCHECK(fetched_config_->value().HasAutomaticSettings());

  ResetProxyConfig(false);

  current_state_ = STATE_WAITING_FOR_INIT_PROXY_RESOLVER;

  init_proxy_resolver_.reset(new InitProxyResolver());
  int rv = init_proxy_resolver_->StartSkipDecider(
      &resolver_, resolver_factory_.get(), effective_config, decider_result,
      script_data,
      base::BindOnce(&ProxyResolutionService::OnInitProxyResolverComplete,
                     base::Unretained(this)));

  if (rv != ERR_IO_PENDING)
    OnInitProxyResolverComplete(rv);
}

void ProxyResolutionService::OnIPAddressChanged() {
  // See the comment block by |kDelayAfterNetworkChangesMs| for info.
  stall_proxy_autoconfig_until_ =
      TimeTicks::Now() + stall_proxy_auto_config_delay_;

  State previous_state = ResetProxyConfig(false);
  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

void ProxyResolutionService::OnDNSChanged() {
  OnIPAddressChanged();
}

}  // namespace net
