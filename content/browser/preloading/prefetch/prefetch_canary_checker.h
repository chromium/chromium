// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CANARY_CHECKER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CANARY_CHECKER_H_

#include <optional>

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "url/gurl.h"

namespace network {
class NetworkConnectionTracker;
}  // namespace network

namespace content {

class BrowserContext;

// This class makes DNS lookups to a specified host to verify if the user's ISP
// would like Prefetch Proxy users to first probe the prefetched host before
// using a prefetched resource. This allows ISP to perform filtering even if
// a response has been fetched via an encrypted tunnel through the Prefetch
// Proxy.
class CONTENT_EXPORT PrefetchCanaryChecker {
 public:
  // Callers who wish to use this class should add a value to this enum. This
  // enum is mapped to a string value which is then used in histograms and
  // prefs. Be sure to update the |PrefetchProxy.CanaryChecker.Clients|
  // histogram suffix in
  // //tools/metrics/histograms/metadata/prefetch/histograms.xml whenever a
  // change is made to this enum.
  //
  // Please add the header file of the client when new items are added.
  enum class CheckType {
    // content/browser/speculation_rules/prefetch/prefetch_origin_decider.h
    kDNS = 0,
    kTLS = 1,
    kMaxValue = kTLS,
  };

  struct CONTENT_EXPORT RetryPolicy {
    RetryPolicy();
    RetryPolicy(const RetryPolicy& other);
    ~RetryPolicy();

    // The maximum number of retries (not including the original check) to
    // attempt.
    size_t max_retries = 0;

    // Backoff policy to use to compute how long we should wait between the end
    // of last retry and start of next retry.
    net::BackoffEntry::Policy backoff_policy = {
        .num_errors_to_ignore = 0,
        .initial_delay_ms = 100,
        .multiply_factor = 2,
        .jitter_factor = 0.2,
        // No maximum backoff.
        .maximum_backoff_ms = -1,
        .entry_lifetime_ms = -1,
        .always_use_initial_delay = false,
    };
  };

  // Cache entry representing a canary check result.
  struct CacheEntry {
    bool success;
    base::Time last_modified;
  };

  // Creates an instance of |PrefetchCanaryChecker| when given a valid |url|. If
  // |url| is invalid then nullptr is returned.
  static std::unique_ptr<PrefetchCanaryChecker> MakePrefetchCanaryChecker(
      BrowserContext* browser_context,
      CheckType,
      const GURL& url,
      const RetryPolicy& retry_policy,
      const base::TimeDelta check_timeout,
      base::TimeDelta revalidate_cache_after);

  PrefetchCanaryChecker(BrowserContext* browser_context,
                        CheckType name,
                        const GURL& url,
                        const RetryPolicy& retry_policy,
                        const base::TimeDelta check_timeout,
                        base::TimeDelta revalidate_cache_after);
  ~PrefetchCanaryChecker();

  PrefetchCanaryChecker(const PrefetchCanaryChecker&) = delete;
  PrefetchCanaryChecker& operator=(const PrefetchCanaryChecker) = delete;

  base::WeakPtr<PrefetchCanaryChecker> GetWeakPtr();

  // Returns the successfulness of the last canary check, if there was one. If
  // the last status was not cached or was cached and needs to be revalidated,
  // this may trigger new checks. This updates the
  // PrefetchProxy.CanaryChecker.CacheLookupStatus histogram, so avoid calling
  // this method repeatedly when its result can be reused.
  std::optional<bool> CanaryCheckSuccessful();

  // Triggers new canary checks if there is no cached status or if the cached
  // status is stale. Use this method over CanaryCheckSuccessful if you only
  // want to freshen the cache (as opposed to look up the cached value), as the
  // CanaryCheckSuccessful method updates the CacheLookupStatus histogram, but
  // RunChecksIfNeeded does not.
  void RunChecksIfNeeded();

  // True if checks are being attempted, including retries.
  bool IsActive() const { return time_when_set_active_.has_value(); }

 private:
  void ResetState();
  void StartDNSResolution(const GURL& url);
  void OnDNSResolved(int net_error,
                     const std::optional<net::AddressList>& resolved_addresses);
  void ProcessTimeout();
  void ProcessFailure(int net_error);
  void ProcessSuccess();
  std::string AppendNameToHistogram(const std::string& histogram) const;
  std::optional<bool> LookupAndRunChecksIfNeeded();
  // Sends a check now if the checker is currently inactive. If the check is
  // active (i.e.: there are DNS resolutions in flight), this is a no-op.
  void SendNowIfInactive();

  // This is called whenever the canary check is done. This is caused whenever
  // the check succeeds, fails and there are no more retries, or the delegate
  // stops the probing.
  void OnCheckEnd(bool success);

  // Updates the cache with the given entry and key. The arguments are in an
  // unusual order to make BindOnce calls easier, as this method is used as a
  // callback since generating the cache key happens asynchronously.
  void UpdateCacheEntry(PrefetchCanaryChecker::CacheEntry entry,
                        std::string key);

  // Simply sets |latest_cache_key_| to |key|. This method is used as a
  // callback since generating the cache key happens asynchronously.
  void UpdateCacheKey(std::string key);

  // The current browser context, not owned.
  raw_ptr<BrowserContext> browser_context_;

  // Pipe to allow cancelling an ongoing DNS resolution request. This is set
  // when we fire off a DNS request to the network service. We send the
  // receiving end to the network service as part of the parameters of the
  // ResolveHost call. The network service then listens to this pipe to
  // potentially cancel the request. The pipe is reset as when the request
  // completes (success or failure).
  mojo::Remote<network::mojom::ResolveHostHandle> resolver_control_handle_;

  // The name given to this checker instance. Used in metrics.
  const std::string name_;

  // The URL that will be DNS resolved.
  const GURL url_;

  // The retry policy to use in this checker.
  const RetryPolicy retry_policy_;

  // The exponential backoff state for retries. This gets reset at the end of
  // each check.
  net::BackoffEntry backoff_entry_;

  // How long before we should timeout a DNS check and retry.
  const base::TimeDelta check_timeout_;

  // How long to allow a cached entry to be valid until it is revalidated in the
  // background.
  const base::TimeDelta revalidate_cache_after_;

  // If a retry is being attempted, this will be running until the next attempt.
  std::unique_ptr<base::OneShotTimer> retry_timer_;

  // If a check is being attempted, this will be running until the TTL.
  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  // Remembers the last time the checker became active.
  std::optional<base::Time> time_when_set_active_;

  // This reference is kept around for unregistering |this| as an observer on
  // any thread.
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  // Small LRU cache holding the result of canary checks made for different
  // networks. This cache is not persisted across browser restarts.
  base::LRUCache<std::string, PrefetchCanaryChecker::CacheEntry> cache_;

  // Keeps track of that latest key used to cache the canary checks. This key
  // changes if the user's network changes. Evaluating the cache key requires
  // an OS lookup which is slow on android, so we store the latest cache key
  // evaluation (and use this stale cache keys for lookups).
  std::string latest_cache_key_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchCanaryChecker> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CANARY_CHECKER_H_
