// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/resolve_context.h"

#include <cstdlib>
#include <limits>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sample_vector.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_server_iterator.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_cache.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/url_request/url_request_context.h"

namespace net {

namespace {

// Min fallback period between queries, in case we are talking to a local DNS
// proxy.
const base::TimeDelta kMinFallbackPeriod = base::Milliseconds(10);

// Default maximum fallback period between queries, even with exponential
// backoff. (Can be overridden by field trial.)
const base::TimeDelta kDefaultMaxFallbackPeriod = base::Seconds(5);

// Maximum RTT that will fit in the RTT histograms.
const base::TimeDelta kRttMax = base::Seconds(30);
// Number of buckets in the histogram of observed RTTs.
const size_t kRttBucketCount = 350;
// Target percentile in the RTT histogram used for fallback period.
const int kRttPercentile = 99;
// Number of samples to seed the histogram with.
const base::HistogramBase::Count kNumSeeds = 2;

DohProviderEntry::List FindDohProvidersMatchingServerConfig(
    DnsOverHttpsServerConfig server_config) {
  DohProviderEntry::List matching_entries;
  for (const DohProviderEntry* entry : DohProviderEntry::GetList()) {
    if (entry->doh_server_config == server_config)
      matching_entries.push_back(entry);
  }

  return matching_entries;
}

DohProviderEntry::List FindDohProvidersAssociatedWithAddress(
    IPAddress server_address) {
  DohProviderEntry::List matching_entries;
  for (const DohProviderEntry* entry : DohProviderEntry::GetList()) {
    if (entry->ip_addresses.count(server_address) > 0)
      matching_entries.push_back(entry);
  }

  return matching_entries;
}

base::TimeDelta GetDefaultFallbackPeriod(const DnsConfig& config) {
  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::GetConnectionType();
  return GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsInitialTimeoutMsByConnectionType", config.fallback_period, type);
}

base::TimeDelta GetMaxFallbackPeriod() {
  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::GetConnectionType();
  return GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
      "AsyncDnsMaxTimeoutMsByConnectionType", kDefaultMaxFallbackPeriod, type);
}

class RttBuckets : public base::BucketRanges {
 public:
  RttBuckets() : base::BucketRanges(kRttBucketCount + 1) {
    base::Histogram::InitializeBucketRanges(
        1,
        base::checked_cast<base::HistogramBase::Sample>(
            kRttMax.InMilliseconds()),
        this);
  }
};

static RttBuckets* GetRttBuckets() {
  static base::NoDestructor<RttBuckets> buckets;
  return buckets.get();
}

static std::unique_ptr<base::SampleVector> GetRttHistogram(
    base::TimeDelta rtt_estimate) {
  std::unique_ptr<base::SampleVector> histogram =
      std::make_unique<base::SampleVector>(GetRttBuckets());
  // Seed histogram with 2 samples at |rtt_estimate|.
  histogram->Accumulate(base::checked_cast<base::HistogramBase::Sample>(
                            rtt_estimate.InMilliseconds()),
                        kNumSeeds);
  return histogram;
}

#if defined(ENABLE_BUILT_IN_DNS)
constexpr size_t kDefaultCacheSize = 1000;
#else
constexpr size_t kDefaultCacheSize = 100;
#endif

std::unique_ptr<HostCache> CreateHostCache(bool enable_caching) {
  if (enable_caching) {
    return std::make_unique<HostCache>(kDefaultCacheSize);
  } else {
    return nullptr;
  }
}

std::unique_ptr<HostResolverCache> CreateHostResolverCache(
    bool enable_caching) {
  if (enable_caching) {
    return std::make_unique<HostResolverCache>(kDefaultCacheSize);
  } else {
    return nullptr;
  }
}

}  // namespace

ResolveContext::ServerStats::ServerStats(
    std::unique_ptr<base::SampleVector> buckets)
    : rtt_histogram(std::move(buckets)) {}

ResolveContext::ServerStats::ServerStats(ServerStats&&) = default;

ResolveContext::ServerStats::~ServerStats() = default;

ResolveContext::ResolveContext(URLRequestContext* url_request_context,
                               bool enable_caching)
    : url_request_context_(url_request_context),
      host_cache_(CreateHostCache(enable_caching)),
      host_resolver_cache_(CreateHostResolverCache(enable_caching)),
      isolation_info_(IsolationInfo::CreateTransient()) {
  max_fallback_period_ = GetMaxFallbackPeriod();
}

ResolveContext::~ResolveContext() = default;

std::unique_ptr<DnsServerIterator> ResolveContext::GetDohIterator(
    const DnsConfig& config,
    const SecureDnsMode& mode,
    const DnsSession* session) {
  // Make the iterator even if the session differs. The first call to the member
  // functions will catch the out of date session.

  return std::make_unique<DohDnsServerIterator>(
      doh_server_stats_.size(), FirstServerIndex(true, session),
      config.doh_attempts, config.attempts, mode, this, session);
}

std::unique_ptr<DnsServerIterator> ResolveContext::GetClassicDnsIterator(
    const DnsConfig& config,
    const DnsSession* session) {
  // Make the iterator even if the session differs. The first call to the member
  // functions will catch the out of date session.

  return std::make_unique<ClassicDnsServerIterator>(
      config.nameservers.size(), FirstServerIndex(false, session),
      config.attempts, config.attempts, this, session);
}

bool ResolveContext::GetDohServerAvailability(size_t doh_server_index,
                                              const DnsSession* session) const {
  if (!IsCurrentSession(session))
    return false;

  CHECK_LT(doh_server_index, doh_server_stats_.size());
  return ServerStatsToDohAvailability(doh_server_stats_[doh_server_index]);
}

size_t ResolveContext::NumAvailableDohServers(const DnsSession* session) const {
  if (!IsCurrentSession(session))
    return 0;

  return base::ranges::count_if(doh_server_stats_,
                                &ServerStatsToDohAvailability);
}

void ResolveContext::RecordServerFailure(size_t server_index,
                                         bool is_doh_server,
                                         int rv,
                                         const DnsSession* session) {
  DCHECK(rv != OK && rv != ERR_NAME_NOT_RESOLVED && rv != ERR_IO_PENDING);

  if (!IsCurrentSession(session))
    return;

  // "FailureError" metric is only recorded for secure queries.
  if (is_doh_server) {
    std::string query_type =
        GetQueryTypeForUma(server_index, true /* is_doh_server */, session);
    DCHECK_NE(query_type, "Insecure");
    std::string provider_id =
        GetDohProviderIdForUma(server_index, true /* is_doh_server */, session);

    base::UmaHistogramSparse(
        base::JoinString(
            {"Net.DNS.DnsTransaction", query_type, provider_id, "FailureError"},
            "."),
        std::abs(rv));
  }

  size_t num_available_doh_servers_before = NumAvailableDohServers(session);

  ServerStats* stats = GetServerStats(server_index, is_doh_server);
  ++(stats->last_failure_count);
  stats->last_failure = base::TimeTicks::Now();
  stats->has_failed_previously = true;

  size_t num_available_doh_servers_now = NumAvailableDohServers(session);
  if (num_available_doh_servers_now < num_available_doh_servers_before) {
    NotifyDohStatusObserversOfUnavailable(false /* network_change */);

    // TODO(crbug.com/40106440): Consider figuring out some way to only for the
    // first context enabling DoH or the last context disabling DoH.
    if (num_available_doh_servers_now == 0)
      NetworkChangeNotifier::TriggerNonSystemDnsChange();
  }
}

void ResolveContext::RecordServerSuccess(size_t server_index,
                                         bool is_doh_server,
                                         const DnsSession* session) {
  if (!IsCurrentSession(session))
    return;

  bool doh_available_before = NumAvailableDohServers(session) > 0;

  ServerStats* stats = GetServerStats(server_index, is_doh_server);
  stats->last_failure_count = 0;
  stats->current_connection_success = true;
  stats->last_failure = base::TimeTicks();
  stats->last_success = base::TimeTicks::Now();

  // TODO(crbug.com/40106440): Consider figuring out some way to only for the
  // first context enabling DoH or the last context disabling DoH.
  bool doh_available_now = NumAvailableDohServers(session) > 0;
  if (doh_available_before != doh_available_now)
    NetworkChangeNotifier::TriggerNonSystemDnsChange();
}

void ResolveContext::RecordRtt(size_t server_index,
                               bool is_doh_server,
                               base::TimeDelta rtt,
                               int rv,
                               const DnsSession* session) {
  if (!IsCurrentSession(session))
    return;

  ServerStats* stats = GetServerStats(server_index, is_doh_server);

  base::TimeDelta base_fallback_period =
      NextFallbackPeriodHelper(stats, 0 /* num_backoffs */);
  RecordRttForUma(server_index, is_doh_server, rtt, rv, base_fallback_period,
                  session);

  // RTT values shouldn't be less than 0, but it shouldn't cause a crash if
  // they are anyway, so clip to 0. See https://crbug.com/753568.
  if (rtt.is_negative())
    rtt = base::TimeDelta();

  // Histogram-based method.
  stats->rtt_histogram->Accumulate(
      base::saturated_cast<base::HistogramBase::Sample>(rtt.InMilliseconds()),
      1);
}

base::TimeDelta ResolveContext::NextClassicFallbackPeriod(
    size_t classic_server_index,
    int attempt,
    const DnsSession* session) {
  if (!IsCurrentSession(session))
    return std::min(GetDefaultFallbackPeriod(session->config()),
                    max_fallback_period_);

  return NextFallbackPeriodHelper(
      GetServerStats(classic_server_index, false /* is _doh_server */),
      attempt / current_session_->config().nameservers.size());
}

base::TimeDelta ResolveContext::NextDohFallbackPeriod(
    size_t doh_server_index,
    const DnsSession* session) {
  if (!IsCurrentSession(session))
    return std::min(GetDefaultFallbackPeriod(session->config()),
                    max_fallback_period_);

  return NextFallbackPeriodHelper(
      GetServerStats(doh_server_index, true /* is _doh_server */),
      0 /* num_backoffs */);
}

base::TimeDelta ResolveContext::ClassicTransactionTimeout(
    const DnsSession* session) {
  if (!IsCurrentSession(session))
    return features::kDnsMinTransactionTimeout.Get();

  // Should not need to call if there are no classic servers configured.
  DCHECK(!classic_server_stats_.empty());

  return TransactionTimeoutHelper(classic_server_stats_.cbegin(),
                                  classic_server_stats_.cend());
}

base::TimeDelta ResolveContext::SecureTransactionTimeout(
    SecureDnsMode secure_dns_mode,
    const DnsSession* session) {
  // Currently only implemented for Secure mode as other modes are assumed to
  // always use aggressive timeouts. If that ever changes, need to implement
  // only accounting for available DoH servers when not Secure mode.
  DCHECK_EQ(secure_dns_mode, SecureDnsMode::kSecure);

  if (!IsCurrentSession(session))
    return features::kDnsMinTransactionTimeout.Get();

  // Should not need to call if there are no DoH servers configured.
  DCHECK(!doh_server_stats_.empty());

  return TransactionTimeoutHelper(doh_server_stats_.cbegin(),
                                  doh_server_stats_.cend());
}

void ResolveContext::RegisterDohStatusObserver(DohStatusObserver* observer) {
  DCHECK(observer);
  doh_status_observers_.AddObserver(observer);
}

void ResolveContext::UnregisterDohStatusObserver(
    const DohStatusObserver* observer) {
  DCHECK(observer);
  doh_status_observers_.RemoveObserver(observer);
}

void ResolveContext::InvalidateCachesAndPerSessionData(
    const DnsSession* new_session,
    bool network_change) {
  // Network-bound ResolveContexts should never receive a cache invalidation due
  // to a network change.
  DCHECK(GetTargetNetwork() == handles::kInvalidNetworkHandle ||
         !network_change);
  if (host_cache_)
    host_cache_->Invalidate();

  // DNS config is constant for any given session, so if the current session is
  // unchanged, any per-session data is safe to keep, even if it's dependent on
  // a specific config.
  if (new_session && new_session == current_session_.get())
    return;

  current_session_.reset();
  doh_autoupgrade_success_metric_timer_.Stop();
  classic_server_stats_.clear();
  doh_server_stats_.clear();
  initial_fallback_period_ = base::TimeDelta();
  max_fallback_period_ = GetMaxFallbackPeriod();

  if (!new_session) {
    NotifyDohStatusObserversOfSessionChanged();
    return;
  }

  current_session_ = new_session->GetWeakPtr();

  initial_fallback_period_ =
      GetDefaultFallbackPeriod(current_session_->config());

  for (size_t i = 0; i < new_session->config().nameservers.size(); ++i) {
    classic_server_stats_.emplace_back(
        GetRttHistogram(initial_fallback_period_));
  }
  for (size_t i = 0; i < new_session->config().doh_config.servers().size();
       ++i) {
    doh_server_stats_.emplace_back(GetRttHistogram(initial_fallback_period_));
  }

  CHECK_EQ(new_session->config().nameservers.size(),
           classic_server_stats_.size());
  CHECK_EQ(new_session->config().doh_config.servers().size(),
           doh_server_stats_.size());

  NotifyDohStatusObserversOfSessionChanged();

  if (!doh_server_stats_.empty())
    NotifyDohStatusObserversOfUnavailable(network_change);
}

void ResolveContext::StartDohAutoupgradeSuccessTimer(
    const DnsSession* session) {
  if (!IsCurrentSession(session)) {
    return;
  }
  if (doh_autoupgrade_success_metric_timer_.IsRunning()) {
    return;
  }
  // We won't pass `session` to `EmitDohAutoupgradeSuccessMetrics()` but will
  // instead reset the timer in `InvalidateCachesAndPerSessionData()` so that
  // the former never gets called after the session changes.
  doh_autoupgrade_success_metric_timer_.Start(
      FROM_HERE, ResolveContext::kDohAutoupgradeSuccessMetricTimeout,
      base::BindOnce(&ResolveContext::EmitDohAutoupgradeSuccessMetrics,
                     base::Unretained(this)));
}

handles::NetworkHandle ResolveContext::GetTargetNetwork() const {
  if (!url_request_context())
    return handles::kInvalidNetworkHandle;

  return url_request_context()->bound_network();
}

size_t ResolveContext::FirstServerIndex(bool doh_server,
                                        const DnsSession* session) {
  if (!IsCurrentSession(session))
    return 0u;

  // DoH first server doesn't rotate, so always return 0u.
  if (doh_server)
    return 0u;

  size_t index = classic_server_index_;
  if (current_session_->config().rotate) {
    classic_server_index_ = (classic_server_index_ + 1) %
                            current_session_->config().nameservers.size();
  }
  return index;
}

bool ResolveContext::IsCurrentSession(const DnsSession* session) const {
  CHECK(session);
  if (session == current_session_.get()) {
    CHECK_EQ(current_session_->config().nameservers.size(),
             classic_server_stats_.size());
    CHECK_EQ(current_session_->config().doh_config.servers().size(),
             doh_server_stats_.size());
    return true;
  }

  return false;
}

ResolveContext::ServerStats* ResolveContext::GetServerStats(
    size_t server_index,
    bool is_doh_server) {
  if (!is_doh_server) {
    CHECK_LT(server_index, classic_server_stats_.size());
    return &classic_server_stats_[server_index];
  } else {
    CHECK_LT(server_index, doh_server_stats_.size());
    return &doh_server_stats_[server_index];
  }
}

base::TimeDelta ResolveContext::NextFallbackPeriodHelper(
    const ServerStats* server_stats,
    int num_backoffs) {
  // Respect initial fallback period (from config or field trial) if it exceeds
  // max.
  if (initial_fallback_period_ > max_fallback_period_)
    return initial_fallback_period_;

  static_assert(std::numeric_limits<base::HistogramBase::Count>::is_signed,
                "histogram base count assumed to be signed");

  // Use fixed percentile of observed samples.
  const base::SampleVector& samples = *server_stats->rtt_histogram;

  base::HistogramBase::Count total = samples.TotalCount();
  base::HistogramBase::Count remaining_count = kRttPercentile * total / 100;
  size_t index = 0;
  while (remaining_count > 0 && index < GetRttBuckets()->size()) {
    remaining_count -= samples.GetCountAtIndex(index);
    ++index;
  }

  base::TimeDelta fallback_period =
      base::Milliseconds(GetRttBuckets()->range(index));

  fallback_period = std::max(fallback_period, kMinFallbackPeriod);

  return std::min(fallback_period * (1 << num_backoffs), max_fallback_period_);
}

template <typename Iterator>
base::TimeDelta ResolveContext::TransactionTimeoutHelper(
    Iterator server_stats_begin,
    Iterator server_stats_end) {
  DCHECK_GE(features::kDnsMinTransactionTimeout.Get(), base::TimeDelta());
  DCHECK_GE(features::kDnsTransactionTimeoutMultiplier.Get(), 0.0);

  // Expect at least one configured server.
  DCHECK(server_stats_begin != server_stats_end);

  base::TimeDelta shortest_fallback_period = base::TimeDelta::Max();
  for (Iterator server_stats = server_stats_begin;
       server_stats != server_stats_end; ++server_stats) {
    shortest_fallback_period = std::min(
        shortest_fallback_period,
        NextFallbackPeriodHelper(&*server_stats, 0 /* num_backoffs */));
  }

  DCHECK_GE(shortest_fallback_period, base::TimeDelta());
  base::TimeDelta ratio_based_timeout =
      shortest_fallback_period *
      features::kDnsTransactionTimeoutMultiplier.Get();

  return std::max(features::kDnsMinTransactionTimeout.Get(),
                  ratio_based_timeout);
}

void ResolveContext::RecordRttForUma(size_t server_index,
                                     bool is_doh_server,
                                     base::TimeDelta rtt,
                                     int rv,
                                     base::TimeDelta base_fallback_period,
                                     const DnsSession* session) {
  DCHECK(IsCurrentSession(session));

  std::string query_type =
      GetQueryTypeForUma(server_index, is_doh_server, session);
  std::string provider_id =
      GetDohProviderIdForUma(server_index, is_doh_server, session);

  // Skip metrics for SecureNotValidated queries unless the provider is tagged
  // for extra logging.
  if (query_type == "SecureNotValidated" &&
      !GetProviderUseExtraLogging(server_index, is_doh_server, session)) {
    return;
  }

  if (rv == OK || rv == ERR_NAME_NOT_RESOLVED) {
    base::UmaHistogramMediumTimes(
        base::JoinString(
            {"Net.DNS.DnsTransaction", query_type, provider_id, "SuccessTime"},
            "."),
        rtt);
  } else {
    base::UmaHistogramMediumTimes(
        base::JoinString(
            {"Net.DNS.DnsTransaction", query_type, provider_id, "FailureTime"},
            "."),
        rtt);
  }
}

std::string ResolveContext::GetQueryTypeForUma(size_t server_index,
                                               bool is_doh_server,
                                               const DnsSession* session) {
  DCHECK(IsCurrentSession(session));

  if (!is_doh_server)
    return "Insecure";

  // Secure queries are validated if the DoH server state is available.
  if (GetDohServerAvailability(server_index, session))
    return "SecureValidated";

  return "SecureNotValidated";
}

std::string ResolveContext::GetDohProviderIdForUma(size_t server_index,
                                                   bool is_doh_server,
                                                   const DnsSession* session) {
  DCHECK(IsCurrentSession(session));

  if (is_doh_server) {
    return GetDohProviderIdForHistogramFromServerConfig(
        session->config().doh_config.servers()[server_index]);
  }

  return GetDohProviderIdForHistogramFromNameserver(
      session->config().nameservers[server_index]);
}

bool ResolveContext::GetProviderUseExtraLogging(size_t server_index,
                                                bool is_doh_server,
                                                const DnsSession* session) {
  DCHECK(IsCurrentSession(session));

  DohProviderEntry::List matching_entries;
  if (is_doh_server) {
    const DnsOverHttpsServerConfig& server_config =
        session->config().doh_config.servers()[server_index];
    matching_entries = FindDohProvidersMatchingServerConfig(server_config);
  } else {
    IPAddress server_address =
        session->config().nameservers[server_index].address();
    matching_entries = FindDohProvidersAssociatedWithAddress(server_address);
  }

  // Use extra logging if any matching provider entries have
  // `LoggingLevel::kExtra` set.
  return base::Contains(matching_entries,
                        DohProviderEntry::LoggingLevel::kExtra,
                        &DohProviderEntry::logging_level);
}

void ResolveContext::NotifyDohStatusObserversOfSessionChanged() {
  for (auto& observer : doh_status_observers_)
    observer.OnSessionChanged();
}

void ResolveContext::NotifyDohStatusObserversOfUnavailable(
    bool network_change) {
  for (auto& observer : doh_status_observers_)
    observer.OnDohServerUnavailable(network_change);
}

void ResolveContext::EmitDohAutoupgradeSuccessMetrics() {
  // This method should not be called if `current_session_` is not populated.
  CHECK(current_session_);

  // If DoH auto-upgrade is not enabled, then don't emit histograms.
  if (current_session_->config().secure_dns_mode != SecureDnsMode::kAutomatic) {
    return;
  }

  DohServerAutoupgradeStatus status;
  for (size_t i = 0; i < doh_server_stats_.size(); i++) {
    auto& entry = doh_server_stats_[i];

    if (ServerStatsToDohAvailability(entry)) {
      if (!entry.has_failed_previously) {
        // Auto-upgrade successful and no prior failures.
        status = DohServerAutoupgradeStatus::kSuccessWithNoPriorFailures;
      } else {
        // Auto-upgrade successful but some prior failures.
        status = DohServerAutoupgradeStatus::kSuccessWithSomePriorFailures;
      }
    } else {
      if (entry.last_success.is_null()) {
        if (entry.last_failure.is_null()) {
          // Skip entries that we've never attempted to use.
          continue;
        }

        // Auto-upgrade failed and DoH requests have never worked. It's possible
        // that an invalid DoH resolver config was provided by the user via
        // enterprise policy (in which case this state will always be associated
        // with the 'Other' provider_id), but it's also possible that there's an
        // issue with the user's network configuration or the provider's
        // infrastructure.
        status = DohServerAutoupgradeStatus::kFailureWithNoPriorSuccesses;
      } else {
        // Auto-upgrade is failing currently but has worked in the past.
        status = DohServerAutoupgradeStatus::kFailureWithSomePriorSuccesses;
      }
    }

    std::string provider_id = GetDohProviderIdForUma(i, /*is_doh_server=*/true,
                                                     current_session_.get());

    base::UmaHistogramEnumeration(
        base::JoinString(
            {"Net.DNS.ResolveContext.DohAutoupgrade", provider_id, "Status"},
            "."),
        status);
  }
}

// static
bool ResolveContext::ServerStatsToDohAvailability(
    const ResolveContext::ServerStats& stats) {
  return stats.last_failure_count < kAutomaticModeFailureLimit &&
         stats.current_connection_success;
}

}  // namespace net
