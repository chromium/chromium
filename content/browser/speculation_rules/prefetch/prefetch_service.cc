// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/cookie_partition_key.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

static ServiceWorkerContext* g_service_worker_context_for_testing = nullptr;

bool (*g_host_non_unique_filter)(base::StringPiece) = nullptr;

}  // namespace

// static
std::unique_ptr<PrefetchService> PrefetchService::CreateIfPossible(
    BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor))
    return nullptr;

  return std::make_unique<PrefetchService>(browser_context);
}

PrefetchService::PrefetchService(BrowserContext* browser_context)
    : browser_context_(browser_context) {}

PrefetchService::~PrefetchService() = default;

void PrefetchService::PrefetchUrl(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(prefetch_container);
  auto prefetch_container_key = prefetch_container->GetPrefetchContainerKey();

  DCHECK(all_prefetches_.find(prefetch_container_key) == all_prefetches_.end());
  all_prefetches_[prefetch_container_key] = prefetch_container;

  CheckEligibilityOfPrefetch(
      prefetch_container,
      base::BindOnce(&PrefetchService::OnGotEligibilityResult,
                     weak_method_factory_.GetWeakPtr()));
}

void PrefetchService::CheckEligibilityOfPrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    OnEligibilityResultCallback result_callback) const {
  DCHECK(prefetch_container);

  // TODO(https://crbug.com/1299059): Clean up the following checks by: 1)
  // moving each check to a separate function, and 2) requiring that failed
  // checks provide a PrefetchStatus related to the check.

  if (browser_context_->IsOffTheRecord()) {
    std::move(result_callback).Run(prefetch_container, false, absl::nullopt);
    return;
  }

  // While a registry-controlled domain could still resolve to a non-publicly
  // routable IP, this allows hosts which are very unlikely to work via the
  // proxy to be discarded immediately.
  bool is_host_non_unique =
      g_host_non_unique_filter
          ? g_host_non_unique_filter(
                prefetch_container->GetURL().HostNoBrackets())
          : net::IsHostnameNonUnique(
                prefetch_container->GetURL().HostNoBrackets());
  if (prefetch_container->GetPrefetchType().IsProxyRequired() &&
      is_host_non_unique) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique);
    return;
  }

  // Only HTTP(S) URLs which are believed to be secure are eligible.
  // For proxied prefetches, we only want HTTPS URLs.
  // For non-proxied prefetches, other URLs (notably localhost HTTP) is also
  // acceptable. This is common during development.
  const bool is_secure_http =
      prefetch_container->GetPrefetchType().IsProxyRequired()
          ? prefetch_container->GetURL().SchemeIs(url::kHttpsScheme)
          : (prefetch_container->GetURL().SchemeIsHTTPOrHTTPS() &&
             network::IsUrlPotentiallyTrustworthy(
                 prefetch_container->GetURL()));
  if (!is_secure_http) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);
    return;
  }

  // TODO(https://crbug.com/1299059): For prefetches that require the proxy,
  // check that the proxy is available and there hasn't been an error received
  // from the proxy recently.

  // Only the default storage partition is supported since that is where we
  // check for service workers and existing cookies.
  StoragePartition* default_storage_partition =
      browser_context_->GetDefaultStoragePartition();
  if (default_storage_partition !=
      browser_context_->GetStoragePartitionForUrl(prefetch_container->GetURL(),
                                                  /*can_create=*/false)) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition);
    return;
  }

  // TODO(https://crbug.com/1299059): Add a delegate to get information from
  // |PrefetchProxyOriginDecider| about whether or not we can prefetch the given
  // origin.

  // This service worker check assumes that the prefetch will only ever be
  // performed in a first-party context (main frame prefetch). At the moment
  // that is true but if it ever changes then the StorageKey will need to be
  // constructed with the top-level site to ensure correct partitioning.
  ServiceWorkerContext* service_worker_context =
      g_service_worker_context_for_testing
          ? g_service_worker_context_for_testing
          : browser_context_->GetDefaultStoragePartition()
                ->GetServiceWorkerContext();
  bool site_has_service_worker =
      service_worker_context->MaybeHasRegistrationForStorageKey(
          blink::StorageKey(url::Origin::Create(prefetch_container->GetURL())));
  if (site_has_service_worker) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker);
    return;
  }

  // We do not need to check the cookies of prefetches that do not need an
  // isolated network context.
  if (!prefetch_container->GetPrefetchType()
           .IsIsolatedNetworkContextRequired()) {
    std::move(result_callback).Run(prefetch_container, true, absl::nullopt);
    return;
  }

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  options.set_return_excluded_cookies();
  default_storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      prefetch_container->GetURL(), options,
      net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&PrefetchService::OnGotCookiesForEligibilityCheck,
                     weak_method_factory_.GetWeakPtr(), prefetch_container,
                     std::move(result_callback)));
}

void PrefetchService::OnGotCookiesForEligibilityCheck(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    OnEligibilityResultCallback result_callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) const {
  if (!prefetch_container) {
    std::move(result_callback).Run(prefetch_container, false, absl::nullopt);
    return;
  }

  if (!cookie_list.empty()) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchNotEligibleUserHasCookies);
    return;
  }

  // Cookies are tricky because cookies for different paths or a higher level
  // domain (e.g.: m.foo.com and foo.com) may not show up in |cookie_list|, but
  // they will show up in |excluded_cookies|. To check for any cookies for a
  // domain, compare the domains of the prefetched |url| and the domains of all
  // the returned cookies.
  bool excluded_cookie_has_tld = false;
  for (const auto& cookie_result : excluded_cookies) {
    if (cookie_result.cookie.IsExpired(base::Time::Now())) {
      // Expired cookies don't count.
      continue;
    }

    if (prefetch_container->GetURL().DomainIs(
            cookie_result.cookie.DomainWithoutDot())) {
      excluded_cookie_has_tld = true;
      break;
    }
  }

  if (excluded_cookie_has_tld) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchNotEligibleUserHasCookies);
    return;
  }

  std::move(result_callback).Run(prefetch_container, true, absl::nullopt);
}

void PrefetchService::OnGotEligibilityResult(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    bool eligible,
    absl::optional<PrefetchStatus> status) const {
  if (!eligible || !prefetch_container) {
    if (status && prefetch_container) {
      prefetch_container->SetPrefetchStatus(status.value());

      // TODO(https://crbug.com/1299059): Consider making this prefetch a decoy.
    }
    return;
  }

  prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchNotStarted);

  // TODO(https://crbug.com/1299059): Add eligible prefetch to a queue, and
  // start the process of prefetching.
}

// static
void PrefetchService::SetServiceWorkerContextForTesting(
    ServiceWorkerContext* context) {
  g_service_worker_context_for_testing = context;
}

// static
void PrefetchService::SetHostNonUniqueFilterForTesting(
    bool (*filter)(base::StringPiece)) {
  g_host_non_unique_filter = filter;
}

}  // namespace content
