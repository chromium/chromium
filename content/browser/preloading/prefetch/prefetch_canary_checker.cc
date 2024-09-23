// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_canary_checker.h"

#include <cmath>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/browser/preloading/prefetch/prefetch_dns_prober.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#include "net/base/network_interfaces.h"
#endif

namespace content {
namespace {

// The maximum number of canary checks to cache. Each entry corresponds to
// a network the user was on during a single Chrome session, and cache misses
// are cheap so there's no reason to use a large value.
const size_t kMaxCacheSize = 4;

const char kFinalResultHistogram[] = "PrefetchProxy.CanaryChecker.FinalState";
const char kTimeUntilSuccess[] = "PrefetchProxy.CanaryChecker.TimeUntilSuccess";
const char kTimeUntilFailure[] = "PrefetchProxy.CanaryChecker.TimeUntilFailure";
const char kAttemptsBeforeSuccessHistogram[] =
    "PrefetchProxy.CanaryChecker.NumAttemptsBeforeSuccess";
const char kNetErrorHistogram[] = "PrefetchProxy.CanaryChecker.NetError";
const char kCacheEntryAgeHistogram[] =
    "PrefetchProxy.CanaryChecker.CacheEntryAge";
const char kCacheLookupResult[] =
    "PrefetchProxy.CanaryChecker.CacheLookupResult";

// These values are persisted to UMA logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CanaryCheckLookupResult {
  kSuccess = 0,
  kFailure = 1,
  kCacheMiss = 2,
  kMaxValue = kCacheMiss,
};

// Please keep this up to date with logged histogram suffix
// |PrefetchProxy.CanaryChecker.Clients| in
// //tools/metrics/histograms/metadata/prefetch/histograms.xml.
std::string NameForClient(PrefetchCanaryChecker::CheckType name) {
  switch (name) {
    case PrefetchCanaryChecker::CheckType::kTLS:
      return "TLS";
    case PrefetchCanaryChecker::CheckType::kDNS:
      return "DNS";
    default:
      NOTREACHED_IN_MIGRATION() << static_cast<int>(name);
      return std::string();
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string GenerateNetworkID(network::mojom::ConnectionType connection_type) {
  std::string id = base::NumberToString(static_cast<int>(connection_type));
  bool is_cellular =
      network::NetworkConnectionTracker::IsConnectionCellular(connection_type);
  if (is_cellular) {
    // Don't care about cell connection type.
    id = "cell";
  }

// Further identify WiFi and cell connections. These calls are only supported
// for Android devices.
#if BUILDFLAG(IS_ANDROID)
  if (connection_type == network::mojom::ConnectionType::CONNECTION_WIFI) {
    return base::StringPrintf("%s,%s", id.c_str(), net::GetWifiSSID().c_str());
  }

  if (is_cellular) {
    return base::StringPrintf(
        "%s,%s", id.c_str(),
        net::android::GetTelephonyNetworkOperator().c_str());
  }
#endif

  return id;
}

void UpdateCacheWithNetworkID(
    network::NetworkConnectionTracker* network_connection_tracker,
    base::OnceCallback<void(std::string key)> updateCallBack) {
  base::OnceCallback<void(network::mojom::ConnectionType connection_type)>
      connectionTypeCallback = base::BindOnce(
          [](base::OnceCallback<void(std::string key)> updateCallBack,
             network::mojom::ConnectionType connection_type) {
            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE, base::BindOnce(GenerateNetworkID, connection_type),
                std::move(updateCallBack));
          },
          std::move(updateCallBack));

  auto split = base::SplitOnceCallback(std::move(connectionTypeCallback));
  network::mojom::ConnectionType connection_type =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  if (network_connection_tracker->GetConnectionType(&connection_type,
                                                    std::move(split.first))) {
    std::move(split.second).Run(connection_type);
  }
}

}  // namespace

PrefetchCanaryChecker::RetryPolicy::RetryPolicy() = default;
PrefetchCanaryChecker::RetryPolicy::~RetryPolicy() = default;
PrefetchCanaryChecker::RetryPolicy::RetryPolicy(
    PrefetchCanaryChecker::RetryPolicy const&) = default;

// static
std::unique_ptr<PrefetchCanaryChecker>
PrefetchCanaryChecker::MakePrefetchCanaryChecker(
    BrowserContext* browser_context,
    CheckType name,
    const GURL& url,
    const RetryPolicy& retry_policy,
    const base::TimeDelta check_timeout,
    base::TimeDelta revalidate_cache_after) {
  if (!url.is_valid()) {
    return nullptr;
  }
  return std::make_unique<PrefetchCanaryChecker>(browser_context, name, url,
                                                 retry_policy, check_timeout,
                                                 revalidate_cache_after);
}

PrefetchCanaryChecker::PrefetchCanaryChecker(
    BrowserContext* browser_context,
    CheckType name,
    const GURL& url,
    const RetryPolicy& retry_policy,
    const base::TimeDelta check_timeout,
    base::TimeDelta revalidate_cache_after)
    : browser_context_(browser_context),
      name_(NameForClient(name)),
      url_(url),
      retry_policy_(retry_policy),
      backoff_entry_(&retry_policy_.backoff_policy),
      check_timeout_(check_timeout),
      revalidate_cache_after_(revalidate_cache_after),
      cache_(kMaxCacheSize) {
  // The NetworkConnectionTracker can only be used directly on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  network_connection_tracker_ = GetNetworkConnectionTracker();
  DCHECK(network_connection_tracker_);
}

PrefetchCanaryChecker::~PrefetchCanaryChecker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::WeakPtr<PrefetchCanaryChecker> PrefetchCanaryChecker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrefetchCanaryChecker::UpdateCacheEntry(
    PrefetchCanaryChecker::CacheEntry entry,
    std::string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  latest_cache_key_ = key;
  cache_.Put(key, entry);
}

void PrefetchCanaryChecker::UpdateCacheKey(std::string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  latest_cache_key_ = key;
}

void PrefetchCanaryChecker::OnCheckEnd(bool success) {
  PrefetchCanaryChecker::CacheEntry entry;
  entry.success = success;
  entry.last_modified = base::Time::Now();

  // We have the check result and we need to store it in the cache, keyed on
  // the current network key. Getting the network key on Android can be slow
  // so we do this asynchronously. Note that this is fundamentally racy: the
  // network might have changed since we completed the check. Fortunately, the
  // impact of using the wrong key is limited: we might simply filter probe when
  // we don't have to or fail to filter probe when we should.
  UpdateCacheWithNetworkID(
      network_connection_tracker_,
      base::BindOnce(&PrefetchCanaryChecker::UpdateCacheEntry, GetWeakPtr(),
                     entry));

  DCHECK(time_when_set_active_.has_value());
  base::TimeDelta active_time =
      base::Time::Now() - time_when_set_active_.value();
  if (success) {
    base::Histogram::FactoryTimeGet(
        AppendNameToHistogram(kTimeUntilSuccess),
        base::Milliseconds(0) /* minimum */,
        base::Milliseconds(30000) /* maximum */, 50 /* bucket_count */,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(active_time.InMilliseconds());
  } else {
    base::Histogram::FactoryTimeGet(
        AppendNameToHistogram(kTimeUntilFailure),
        base::Milliseconds(0) /* minimum */,
        base::Milliseconds(60000) /* maximum */, 50 /* bucket_count */,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(active_time.InMilliseconds());
  }
  base::BooleanHistogram::FactoryGet(
      AppendNameToHistogram(kFinalResultHistogram),
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(success);

  ResetState();
}

void PrefetchCanaryChecker::ResetState() {
  time_when_set_active_ = std::nullopt;
  resolver_control_handle_.reset();
  retry_timer_.reset();
  timeout_timer_.reset();
  backoff_entry_.Reset();
}

void PrefetchCanaryChecker::SendNowIfInactive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (time_when_set_active_.has_value()) {
    // We already have an active check.
    return;
  }
  time_when_set_active_ = base::Time::Now();

  StartDNSResolution(url_);
}

void PrefetchCanaryChecker::ProcessTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel the pending resolving job. This will do nothing if resolving has
  // already completed. Otherwise, the callback we registered (OnDNSResolved)
  // will be called with the error code we pass here (net::ERR_TIMED_OUT).
  resolver_control_handle_->Cancel(net::ERR_TIMED_OUT);
}

void PrefetchCanaryChecker::ProcessFailure(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(time_when_set_active_.has_value());

  backoff_entry_.InformOfRequest(false);

  base::UmaHistogramSparse(AppendNameToHistogram(kNetErrorHistogram),
                           std::abs(net_error));

  if (retry_policy_.max_retries >=
      static_cast<size_t>(backoff_entry_.failure_count())) {
    base::TimeDelta interval = backoff_entry_.GetTimeUntilRelease();

    retry_timer_ = std::make_unique<base::OneShotTimer>();
    // base::Unretained is safe because |retry_timer_| is owned by this.
    retry_timer_->Start(
        FROM_HERE, interval,
        base::BindOnce(&PrefetchCanaryChecker::StartDNSResolution,
                       base::Unretained(this), url_));
    return;
  }

  OnCheckEnd(false);
}

void PrefetchCanaryChecker::ProcessSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(time_when_set_active_.has_value());

  base::LinearHistogram::FactoryGet(
      AppendNameToHistogram(kAttemptsBeforeSuccessHistogram), 1 /* minimum */,
      25 /* maximum */, 25 /* bucket_count */,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      // |failure_count| is zero when the first attempt is successful.
      // Increase by one for more intuitive metrics.
      ->Add(backoff_entry_.failure_count() + 1);

  OnCheckEnd(true);
}

std::optional<bool> PrefetchCanaryChecker::CanaryCheckSuccessful() {
  std::optional<bool> result = LookupAndRunChecksIfNeeded();
  CanaryCheckLookupResult result_enum;
  if (!result.has_value()) {
    result_enum = CanaryCheckLookupResult::kCacheMiss;
  } else if (result.value()) {
    result_enum = CanaryCheckLookupResult::kSuccess;
  } else {
    result_enum = CanaryCheckLookupResult::kFailure;
  }

  base::UmaHistogramEnumeration(AppendNameToHistogram(kCacheLookupResult),
                                result_enum);
  return result;
}

// RunChecksIfNeeded is the public version of LookupAndRunChecksIfNeeded that
// doesn't return the lookup value, to force clients to use
// CanaryCheckSuccessful (which reports UMA) for lookups.
void PrefetchCanaryChecker::RunChecksIfNeeded() {
  LookupAndRunChecksIfNeeded();
}

std::optional<bool> PrefetchCanaryChecker::LookupAndRunChecksIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Asynchronously update the network cache key. On Android, getting the
  // network cache key can be very slow, so we don't want to block the main
  // thread.
  UpdateCacheWithNetworkID(
      network_connection_tracker_,
      base::BindOnce(&PrefetchCanaryChecker::UpdateCacheKey, GetWeakPtr()));

  // Assume the cache key has not changed since last time we checked it. Note
  // that if we have never set latest_cache_key_, |it| will be cache_.end().
  auto it = cache_.Get(latest_cache_key_);
  if (it == cache_.end()) {
    SendNowIfInactive();
    return std::optional<bool>();
  }

  const PrefetchCanaryChecker::CacheEntry& entry = it->second;
  base::TimeDelta cache_entry_age = base::Time::Now() - entry.last_modified;

  base::LinearHistogram::FactoryTimeGet(
      AppendNameToHistogram(kCacheEntryAgeHistogram),
      base::Hours(0) /* minimum */, base::Hours(72) /* maximum */,
      50 /* bucket_count */, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(cache_entry_age.InHours());

  // Check if the cache entry should be revalidated because it has expired or
  // cache_entry_age is negative because the clock was moved back.
  if (cache_entry_age >= revalidate_cache_after_ ||
      cache_entry_age.is_negative()) {
    SendNowIfInactive();
  }

  return entry.success;
}

std::string PrefetchCanaryChecker::AppendNameToHistogram(
    const std::string& histogram) const {
  return base::StringPrintf("%s.%s", histogram.c_str(), name_.c_str());
}

void PrefetchCanaryChecker::StartDNSResolution(const GURL& url) {
  net::NetworkAnonymizationKey nak =
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url))
          .network_anonymization_key();

  network::mojom::ResolveHostParametersPtr resolve_host_parameters =
      network::mojom::ResolveHostParameters::New();
  resolve_host_parameters->initial_priority = net::RequestPriority::IDLE;
  // Don't use DNS results cached at the user's device.
  resolve_host_parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  // Allow cancelling the request.
  resolver_control_handle_ = mojo::Remote<network::mojom::ResolveHostHandle>();
  resolve_host_parameters->control_handle =
      resolver_control_handle_.BindNewPipeAndPassReceiver();

  mojo::PendingRemote<network::mojom::ResolveHostClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<PrefetchDNSProber>(
          base::BindOnce(&PrefetchCanaryChecker::OnDNSResolved, GetWeakPtr())),
      client_remote.InitWithNewPipeAndPassReceiver());

  // TODO(crbug.com/40235854): Consider passing a SchemeHostPort to trigger
  // HTTPS DNS resource record query.
  browser_context_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                        net::HostPortPair::FromURL(url)),
                    nak, std::move(resolve_host_parameters),
                    std::move(client_remote));

  timeout_timer_ = std::make_unique<base::OneShotTimer>();
  // base::Unretained is safe because |timeout_timer_| is owned by this.
  timeout_timer_->Start(FROM_HERE, check_timeout_,
                        base::BindOnce(&PrefetchCanaryChecker::ProcessTimeout,
                                       base::Unretained(this)));
}

void PrefetchCanaryChecker::OnDNSResolved(
    int net_error,
    const std::optional<net::AddressList>& resolved_addresses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timeout_timer_.reset();
  resolver_control_handle_.reset();
  bool successful = net_error == net::OK && resolved_addresses &&
                    !resolved_addresses->empty();
  if (successful) {
    ProcessSuccess();
  } else {
    ProcessFailure(net_error);
  }
}

}  // namespace content
