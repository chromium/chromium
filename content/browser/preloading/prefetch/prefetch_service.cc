// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_service.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_proxy_configurator.h"
#include "content/browser/preloading/prefetch/prefetched_mainframe_response_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/cookie_partition_key.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

static ServiceWorkerContext* g_service_worker_context_for_testing = nullptr;

bool (*g_host_non_unique_filter)(base::StringPiece) = nullptr;

static network::mojom::URLLoaderFactory* g_url_loader_factory_for_testing =
    nullptr;

bool ShouldConsiderDecoyRequestForStatus(PrefetchStatus status) {
  switch (status) {
    case PrefetchStatus::kPrefetchNotEligibleUserHasCookies:
    case PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
      // If the prefetch is not eligible because of cookie or a service worker,
      // then maybe send a decoy.
      return true;
    case PrefetchStatus::kPrefetchNotEligibleGoogleDomain:
    case PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchPositionIneligible:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchProxyNotAvailable:
    case PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
      // These statuses don't relate to any user state, so don't send a decoy
      // request.
      return false;
    case PrefetchStatus::kPrefetchUsedNoProbe:
    case PrefetchStatus::kPrefetchUsedProbeSuccess:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchNotFinishedInTime:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchSuccessful:
    case PrefetchStatus::kNavigatedToLinkNotOnSRP:
    case PrefetchStatus::kSubresourceThrottled:
    case PrefetchStatus::kPrefetchUsedNoProbeWithNSP:
    case PrefetchStatus::kPrefetchUsedProbeSuccessWithNSP:
    case PrefetchStatus::kPrefetchNotUsedProbeFailedWithNSP:
    case PrefetchStatus::kPrefetchUsedNoProbeNSPAttemptDenied:
    case PrefetchStatus::kPrefetchUsedProbeSuccessNSPAttemptDenied:
    case PrefetchStatus::kPrefetchNotUsedProbeFailedNSPAttemptDenied:
    case PrefetchStatus::kPrefetchUsedNoProbeNSPNotStarted:
    case PrefetchStatus::kPrefetchUsedProbeSuccessNSPNotStarted:
    case PrefetchStatus::kPrefetchNotUsedProbeFailedNSPNotStarted:
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchIsStaleWithNSP:
    case PrefetchStatus::kPrefetchIsStaleNSPAttemptDenied:
    case PrefetchStatus::kPrefetchIsStaleNSPNotStarted:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchFailedRedirectsDisabled:
      // These statuses should not be returned by the eligibility checks, and
      // thus not be passed in here.
      NOTREACHED();
      return false;
  }
}

bool ShouldStartSpareRenderer() {
  if (!PrefetchStartsSpareRenderer()) {
    return false;
  }

  for (RenderProcessHost::iterator iter(RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->IsUnused()) {
      // There is already a spare renderer.
      return false;
    }
  }
  return true;
}

absl::optional<base::TimeDelta> GetTotalPrefetchTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::Time start = head->request_time;
  base::Time end = head->response_time;

  if (start.is_null() || end.is_null())
    return absl::nullopt;
  return end - start;
}

absl::optional<base::TimeDelta> GetPrefetchConnectTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::TimeTicks start = head->load_timing.connect_timing.connect_start;
  base::TimeTicks end = head->load_timing.connect_timing.connect_end;

  if (start.is_null() || end.is_null())
    return absl::nullopt;
  return end - start;
}

void RecordPrefetchProxyPrefetchMainframeCookiesToCopy(
    size_t cookie_list_size) {
  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.Mainframe.CookiesToCopy",
                           cookie_list_size);
}

void CookieSetHelper(base::RepeatingClosure closure,
                     net::CookieAccessResult access_result) {
  closure.Run();
}

}  // namespace

// static
std::unique_ptr<PrefetchService> PrefetchService::CreateIfPossible(
    BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor))
    return nullptr;

  return std::make_unique<PrefetchService>(browser_context);
}

PrefetchService::PrefetchService(BrowserContext* browser_context)
    : browser_context_(browser_context),
      delegate_(GetContentClient()->browser()->CreatePrefetchServiceDelegate(
          browser_context_)),
      prefetch_proxy_configurator_(
          PrefetchProxyConfigurator::MaybeCreatePrefetchProxyConfigurator(
              PrefetchProxyHost(delegate_
                                    ? delegate_->GetDefaultPrefetchProxyHost()
                                    : GURL("")),
              delegate_ ? delegate_->GetAPIKey() : "")),
      origin_prober_(std::make_unique<PrefetchOriginProber>(
          browser_context_,
          PrefetchDNSCanaryCheckURL(
              delegate_ ? delegate_->GetDefaultDNSCanaryCheckURL() : GURL("")),
          PrefetchTLSCanaryCheckURL(
              delegate_ ? delegate_->GetDefaultTLSCanaryCheckURL()
                        : GURL("")))) {}

PrefetchService::~PrefetchService() = default;

void PrefetchService::PrefetchUrl(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(prefetch_container);
  auto prefetch_container_key = prefetch_container->GetPrefetchContainerKey();

  // If the user has disabled pre* actions, then don't prefetch.
  if (delegate_ && !delegate_->IsSomePreloadingEnabled()) {
    return;
  }

  if (delegate_) {
    bool allow_all_domains = PrefetchAllowAllDomains() ||
                             (PrefetchAllowAllDomainsForExtendedPreloading() &&
                              delegate_->IsExtendedPreloadingEnabled());
    if (!allow_all_domains &&
        !delegate_->IsDomainInPrefetchAllowList(
            RenderFrameHost::FromID(
                prefetch_container->GetReferringRenderFrameHostId())
                ->GetLastCommittedURL())) {
      return;
    }
  }

  RecordExistingPrefetchWithMatchingURL(prefetch_container);

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

  if (GetContentClient()->browser()->IsDataSaverEnabled(browser_context_)) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled);
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
  if (!prefetch_container->GetPrefetchType().IsProxyBypassedForTesting() &&
      prefetch_container->GetPrefetchType().IsProxyRequired() &&
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

  if (prefetch_container->GetPrefetchType().IsProxyRequired() &&
      !prefetch_container->GetPrefetchType().IsProxyBypassedForTesting() &&
      (!prefetch_proxy_configurator_ ||
       !prefetch_proxy_configurator_->IsPrefetchProxyAvailable())) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchProxyNotAvailable);
    return;
  }

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

  // If we have recently received a "retry-after" for the origin, then don't
  // send new prefetches.
  if (delegate_ && !delegate_->IsOriginOutsideRetryAfterWindow(
                       prefetch_container->GetURL())) {
    std::move(result_callback)
        .Run(prefetch_container, false,
             PrefetchStatus::kPrefetchIneligibleRetryAfter);
    return;
  }

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
    absl::optional<PrefetchStatus> status) {
  if (!eligible || !prefetch_container) {
    if (status && prefetch_container) {
      prefetch_container->SetPrefetchStatus(status.value());

      if (prefetch_container->GetPrefetchType().IsProxyRequired() &&
          ShouldConsiderDecoyRequestForStatus(
              prefetch_container->GetPrefetchStatus()) &&
          PrefetchServiceSendDecoyRequestForIneligblePrefetch(
              delegate_ ? delegate_->DisableDecoysBasedOnUserSettings()
                        : false)) {
        prefetch_container->SetIsDecoy(true);
        prefetch_queue_.push_back(prefetch_container);
        prefetch_container->SetPrefetchStatus(
            PrefetchStatus::kPrefetchIsPrivacyDecoy);
        Prefetch();
      }
    }
    return;
  }

  prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchNotStarted);
  prefetch_queue_.push_back(prefetch_container);

  Prefetch();

  // Registers a cookie listener for this prefetch if it is using an isolated
  // network context. If the cookies in the default partition associated with
  // this URL change after this point, then the prefetched resources should not
  // be served.
  if (prefetch_container->GetPrefetchType()
          .IsIsolatedNetworkContextRequired()) {
    prefetch_container->RegisterCookieListener(
        browser_context_->GetDefaultStoragePartition()
            ->GetCookieManagerForBrowserProcess());
  }
}

void PrefetchService::Prefetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (PrefetchCloseIdleSockets()) {
    for (const auto& iter : all_prefetches_) {
      if (iter.second && iter.second->GetNetworkContext()) {
        iter.second->GetNetworkContext()->CloseIdleConnections();
      }
    }
  }

  base::WeakPtr<PrefetchContainer> next_prefetch = nullptr;
  while ((next_prefetch = PopNextPrefetchContainer()) != nullptr) {
    StartSinglePrefetch(next_prefetch);
  }
}

base::WeakPtr<PrefetchContainer> PrefetchService::PopNextPrefetchContainer() {
  // Remove all prefetches from queue that no longer exist.
  auto new_end = std::remove_if(
      prefetch_queue_.begin(), prefetch_queue_.end(),
      [](const base::WeakPtr<PrefetchContainer>& prefetch_container) {
        return !prefetch_container;
      });
  prefetch_queue_.erase(new_end, prefetch_queue_.end());

  // TODO(https://crbug.com/1299059): Remove prefetches from queue once the
  // number of prefetches started by its referring render frame host exceeds
  // some maximum limit.

  // Don't start any new prefetches if we are currently at or beyond the limit
  // for the number of concurrent prefetches.
  DCHECK(num_active_prefetches_ >= 0);
  DCHECK(PrefetchServiceMaximumNumberOfConcurrentPrefetches() >= 0);
  if (num_active_prefetches_ >=
      PrefetchServiceMaximumNumberOfConcurrentPrefetches()) {
    return nullptr;
  }

  // Get the first prefetch that is from an active render frame host and in a
  // visible WebContents.
  auto prefetch_iter = std::find_if(
      prefetch_queue_.begin(), prefetch_queue_.end(),
      [](const base::WeakPtr<PrefetchContainer>& prefetch_container) {
        RenderFrameHost* rfh = RenderFrameHost::FromID(
            prefetch_container->GetReferringRenderFrameHostId());
        return rfh->IsActive() && rfh->GetPage().IsPrimary() &&
               WebContents::FromRenderFrameHost(rfh)->GetVisibility() ==
                   Visibility::VISIBLE;
      });
  if (prefetch_iter == prefetch_queue_.end()) {
    return nullptr;
  }

  base::WeakPtr<PrefetchContainer> next_prefetch_container = *prefetch_iter;
  prefetch_queue_.erase(prefetch_iter);

  return next_prefetch_container;
}

void PrefetchService::TakeOwnershipOfPrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(prefetch_container);

  // Take ownership of the |PrefetchContainer| from the
  // |PrefetchDocumentManager|.
  PrefetchDocumentManager* prefetch_document_manager =
      prefetch_container->GetPrefetchDocumentManager();
  DCHECK(prefetch_document_manager);
  std::unique_ptr<PrefetchContainer> owned_prefetch_container =
      prefetch_document_manager->ReleasePrefetchContainer(
          prefetch_container->GetURL());
  DCHECK(owned_prefetch_container.get() == prefetch_container.get());

  // Create callback to delete the prefetch container after
  // |PrefetchContainerLifetimeInPrefetchService|.
  base::TimeDelta reset_delta = PrefetchContainerLifetimeInPrefetchService();
  std::unique_ptr<base::OneShotTimer> reset_callback = nullptr;
  if (reset_delta.is_positive()) {
    reset_callback = std::make_unique<base::OneShotTimer>();
    reset_callback->Start(
        FROM_HERE, PrefetchContainerLifetimeInPrefetchService(),
        base::BindOnce(&PrefetchService::ResetPrefetch, base::Unretained(this),
                       prefetch_container));
  }

  // Store prefetch and callback to delete prefetch.
  owned_prefetches_[prefetch_container->GetPrefetchContainerKey()] =
      std::make_pair(std::move(owned_prefetch_container),
                     std::move(reset_callback));
}

void PrefetchService::ResetPrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(prefetch_container);
  DCHECK(
      owned_prefetches_.find(prefetch_container->GetPrefetchContainerKey()) !=
      owned_prefetches_.end());
  owned_prefetches_.erase(
      owned_prefetches_.find(prefetch_container->GetPrefetchContainerKey()));

  auto prefetches_ready_to_serve_iter =
      prefetches_ready_to_serve_.find(prefetch_container->GetURL());
  if (prefetches_ready_to_serve_iter != prefetches_ready_to_serve_.end() &&
      prefetches_ready_to_serve_iter->second->GetPrefetchContainerKey() ==
          prefetch_container->GetPrefetchContainerKey()) {
    prefetches_ready_to_serve_.erase(prefetches_ready_to_serve_iter);
  }
}

void PrefetchService::StartSinglePrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(prefetch_container);

  TakeOwnershipOfPrefetch(prefetch_container);

  if (!prefetch_container->IsDecoy()) {
    // The status is updated to be successful or failed when it finishes.
    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchNotFinishedInTime);
  }

  url::Origin origin = url::Origin::Create(prefetch_container->GetURL());
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));
  network::ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info = isolation_info;

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = prefetch_container->GetURL();
  request->method = "GET";
  request->enable_load_timing = true;
  // TODO(https://crbug.com/1317756): Investigate if we need to include the
  // net::LOAD_DISABLE_CACHE flag.
  request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH;
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  request->headers.SetHeader(kCorsExemptPurposeHeaderName, "prefetch");
  request->headers.SetHeader(
      "Sec-Purpose", prefetch_container->GetPrefetchType().IsProxyRequired()
                         ? "prefetch;anonymous-client-ip"
                         : "prefetch");

  // Remove the user agent header if it was set so that the network context's
  // default is used.
  request->headers.RemoveHeader("User-Agent");
  request->trusted_params = trusted_params;
  request->site_for_cookies = trusted_params.isolation_info.site_for_cookies();
  request->devtools_request_id = prefetch_container->RequestId();

  const auto& devtools_observer = prefetch_container->GetDevToolsObserver();
  if (devtools_observer && !prefetch_container->IsDecoy()) {
    request->trusted_params->devtools_observer =
        devtools_observer->MakeSelfOwnedNetworkServiceDevToolsObserver();
    devtools_observer->OnStartSinglePrefetch(prefetch_container->RequestId(),
                                             *request);
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("speculation_rules_prefetch",
                                          R"(
          semantics {
            sender: "Speculation Rules Prefetch Loader"
            description:
              "Prefetches the mainframe HTML of a page specified via "
              "speculation rules. This is done out-of-band of normal "
              "prefetches to allow total isolation of this request from the "
              "rest of browser traffic and user state like cookies and cache."
            trigger:
              "Used only when this feature and speculation rules feature are "
              "enabled."
            data: "None."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control this via a setting specific to each content "
              "embedder."
            policy_exception_justification: "Not implemented."
        })");

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  loader->SetOnRedirectCallback(
      base::BindRepeating(&PrefetchService::OnPrefetchRedirect,
                          base::Unretained(this), prefetch_container));
  loader->SetAllowHttpErrorResults(true);
  loader->SetTimeoutDuration(PrefetchTimeoutDuration());
  loader->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
      network::mojom::kURLLoadOptionSniffMimeType |
      network::mojom::kURLLoadOptionSendSSLInfoForCertificateError);
  loader->DownloadToString(GetURLLoaderFactory(prefetch_container),
                           base::BindOnce(&PrefetchService::OnPrefetchComplete,
                                          base::Unretained(this),
                                          prefetch_container, isolation_info),
                           PrefetchMainframeBodyLengthLimit());
  prefetch_container->TakeURLLoader(std::move(loader));
  num_active_prefetches_++;

  PrefetchDocumentManager* prefetch_document_manager =
      prefetch_container->GetPrefetchDocumentManager();
  if (!prefetch_container->IsDecoy() &&
      (!prefetch_document_manager ||
       !prefetch_document_manager->HaveCanaryChecksStarted())) {
    // Make sure canary checks have run so we know the result by the time we
    // want to use the prefetch. Checking the canary cache can be a slow and
    // blocking operation (see crbug.com/1266018), so we only do this for the
    // first non-decoy prefetch we make on the page.
    // TODO(crbug.com/1266018): once this bug is fixed, fire off canary check
    // regardless of whether the request is a decoy or not.
    origin_prober_->RunCanaryChecksIfNeeded();

    if (prefetch_document_manager)
      prefetch_document_manager->OnCanaryChecksStarted();
  }

  // Start a spare renderer now so that it will be ready by the time it is
  // useful to have.
  if (ShouldStartSpareRenderer()) {
    RenderProcessHost::WarmupSpareRenderProcessHost(browser_context_);
  }
}

network::mojom::URLLoaderFactory* PrefetchService::GetURLLoaderFactory(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(prefetch_container);
  if (g_url_loader_factory_for_testing) {
    return g_url_loader_factory_for_testing;
  }
  return prefetch_container->GetOrCreateNetworkContext(this)
      ->GetURLLoaderFactory();
}

void PrefetchService::OnPrefetchRedirect(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* removed_headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  num_active_prefetches_--;

  if (!prefetch_container)
    return;

  // Currently all redirects are disabled. See https://crbug.com/1266876 for
  // more details.
  prefetch_container->SetPrefetchStatus(
      PrefetchStatus::kPrefetchFailedRedirectsDisabled);

  // Cancels current request.
  prefetch_container->ResetURLLoader();

  // Send DevTools event
  const auto& devtools_observer = prefetch_container->GetDevToolsObserver();
  if (devtools_observer) {
    devtools_observer->OnPrefetchResponseReceived(
        prefetch_container->GetURL(), prefetch_container->RequestId(),
        response_head);

    devtools_observer->OnPrefetchRequestComplete(
        prefetch_container->RequestId(),
        network::URLLoaderCompletionStatus{net::ERR_NOT_IMPLEMENTED});
  }

  // Continue prefetching other URLs.
  Prefetch();
}

void PrefetchService::OnPrefetchComplete(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    const net::IsolationInfo& isolation_info,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  num_active_prefetches_--;

  if (!prefetch_container)
    return;

  // TODO(https://crbug.com/1299059): Store relevant metrics based on the status
  // of the completed prefetch.

  if (prefetch_container->IsDecoy()) {
    // Since this prefetch was a decoy, we don't cache the response.
    prefetch_container->ResetURLLoader();
    Prefetch();
    return;
  }

  base::UmaHistogramSparse(
      "PrefetchProxy.Prefetch.Mainframe.NetError",
      std::abs(prefetch_container->GetLoader()->NetError()));

  const auto& devtools_observer = prefetch_container->GetDevToolsObserver();
  if (devtools_observer) {
    if (prefetch_container->GetLoader()->ResponseInfo()) {
      devtools_observer->OnPrefetchResponseReceived(
          prefetch_container->GetURL(), prefetch_container->RequestId(),
          *prefetch_container->GetLoader()->ResponseInfo());
    }

    if (body) {
      devtools_observer->OnPrefetchBodyDataReceived(
          prefetch_container->RequestId(), *body, /*is_base64_encoded=*/false);
    }

    devtools_observer->OnPrefetchRequestComplete(
        prefetch_container->RequestId(),
        prefetch_container->GetLoader()->CompletionStatus().value_or(
            network::URLLoaderCompletionStatus(
                prefetch_container->GetLoader()->NetError())));
  }

  if (prefetch_container->GetLoader()->NetError() != net::OK) {
    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchFailedNetError);
  }

  if (prefetch_container->GetLoader()->NetError() == net::OK && body &&
      prefetch_container->GetLoader()->ResponseInfo()) {
    network::mojom::URLResponseHeadPtr head =
        prefetch_container->GetLoader()->ResponseInfo()->Clone();

    // Verifies that the request was made using the prefetch proxy if required,
    // or made directly if the proxy was not required.
    DCHECK(prefetch_container->GetPrefetchType().IsProxyBypassedForTesting() ||
           !head->proxy_server.is_direct() ==
               prefetch_container->GetPrefetchType().IsProxyRequired());

    HandlePrefetchedResponse(prefetch_container, isolation_info,
                             std::move(head), std::move(body));
  }

  prefetch_container->ResetURLLoader();
  Prefetch();
}

void PrefetchService::HandlePrefetchedResponse(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    const net::IsolationInfo& isolation_info,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body) {
  DCHECK(prefetch_container);
  DCHECK(!head->was_fetched_via_cache);

  if (!head->headers)
    return;

  UMA_HISTOGRAM_COUNTS_10M("PrefetchProxy.Prefetch.Mainframe.BodyLength",
                           body->size());

  absl::optional<base::TimeDelta> total_time = GetTotalPrefetchTime(head.get());
  if (total_time) {
    UMA_HISTOGRAM_CUSTOM_TIMES("PrefetchProxy.Prefetch.Mainframe.TotalTime",
                               *total_time, base::Milliseconds(10),
                               base::Seconds(30), 100);
  }

  absl::optional<base::TimeDelta> connect_time =
      GetPrefetchConnectTime(head.get());
  if (connect_time) {
    UMA_HISTOGRAM_TIMES("PrefetchProxy.Prefetch.Mainframe.ConnectTime",
                        *connect_time);
  }

  int response_code = head->headers->response_code();

  base::UmaHistogramSparse("PrefetchProxy.Prefetch.Mainframe.RespCode",
                           response_code);
  if (response_code < 200 | response_code >= 300) {
    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchFailedNon2XX);

    if (response_code == net::HTTP_SERVICE_UNAVAILABLE) {
      base::TimeDelta retry_after;
      std::string retry_after_string;
      if (head->headers->EnumerateHeader(nullptr, "Retry-After",
                                         &retry_after_string) &&
          net::HttpUtil::ParseRetryAfterHeader(
              retry_after_string, base::Time::Now(), &retry_after) &&
          delegate_) {
        delegate_->ReportOriginRetryAfter(prefetch_container->GetURL(),
                                          retry_after);
      }
    }
    return;
  }

  if (PrefetchServiceHTMLOnly() && head->mime_type != "text/html") {
    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchFailedMIMENotSupported);
    return;
  }

  prefetch_container->TakePrefetchedResponse(
      std::make_unique<PrefetchedMainframeResponseContainer>(
          isolation_info, std::move(head), std::move(body)));
  prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchSuccessful);
}

void PrefetchService::PrepareToServe(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  // Ensure |this| has this prefetch.
  if (all_prefetches_.find(prefetch_container->GetPrefetchContainerKey()) ==
      all_prefetches_.end())
    return;

  // If the prefetch isn't ready to be served, then stop.
  if (prefetch_container->HaveDefaultContextCookiesChanged() ||
      !prefetch_container->HasValidPrefetchedResponse(
          PrefetchCacheableDuration()))
    return;

  // If the prefetch has a valid response, then it must be in
  // |owned_prefetches_|.
  DCHECK(
      owned_prefetches_.find(prefetch_container->GetPrefetchContainerKey()) !=
      owned_prefetches_.end());

  // If there is already a prefetch with the same URL as |prefetch_container| in
  // |prefetches_ready_to_serve_|, then don't do anything.
  if (prefetches_ready_to_serve_.find(prefetch_container->GetURL()) !=
      prefetches_ready_to_serve_.end())
    return;

  // Move prefetch into |prefetches_ready_to_serve_|.
  prefetches_ready_to_serve_[prefetch_container->GetURL()] = prefetch_container;

  // Start the process of copying cookies from the isolated network context used
  // to make the prefetch to the default network context.
  CopyIsolatedCookies(prefetch_container);
}

void PrefetchService::CopyIsolatedCookies(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(prefetch_container);

  if (!prefetch_container->GetNetworkContext()) {
    // Not set in unit tests.
    return;
  }

  // We only need to copy cookies if the prefetch used an isolated network
  // context.
  if (!prefetch_container->GetPrefetchType()
           .IsIsolatedNetworkContextRequired()) {
    return;
  }

  prefetch_container->OnIsolatedCookieCopyStart();
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  prefetch_container->GetNetworkContext()->GetCookieManager()->GetCookieList(
      prefetch_container->GetURL(), options,
      net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&PrefetchService::OnGotIsolatedCookiesForCopy,
                     weak_method_factory_.GetWeakPtr(), prefetch_container));
}

void PrefetchService::OnGotIsolatedCookiesForCopy(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  RecordPrefetchProxyPrefetchMainframeCookiesToCopy(cookie_list.size());

  if (cookie_list.empty()) {
    prefetch_container->OnIsolatedCookieCopyComplete();
    return;
  }

  base::RepeatingClosure barrier = base::BarrierClosure(
      cookie_list.size(),
      base::BindOnce(&PrefetchContainer::OnIsolatedCookieCopyComplete,
                     prefetch_container));

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  for (const net::CookieWithAccessResult& cookie : cookie_list) {
    browser_context_->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->SetCanonicalCookie(cookie.cookie, prefetch_container->GetURL(),
                             options,
                             base::BindOnce(&CookieSetHelper, barrier));
  }
}

base::WeakPtr<PrefetchContainer> PrefetchService::GetPrefetchToServe(
    const GURL& url) const {
  auto prefetch_iter = prefetches_ready_to_serve_.find(url);

  if (prefetch_iter == prefetches_ready_to_serve_.end())
    return nullptr;

  return prefetch_iter->second;
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

// static
void PrefetchService::SetURLLoaderFactoryForTesting(
    network::mojom::URLLoaderFactory* url_loader_factory) {
  g_url_loader_factory_for_testing = url_loader_factory;
}

void PrefetchService::RecordExistingPrefetchWithMatchingURL(
    base::WeakPtr<PrefetchContainer> prefetch_container) const {
  bool matching_prefetch = false;
  for (const auto& prefetch_iter : all_prefetches_) {
    if (prefetch_iter.second &&
        prefetch_iter.second->GetURL() == prefetch_container->GetURL() &&
        prefetch_iter.second->GetReferringRenderFrameHostId() !=
            prefetch_container->GetReferringRenderFrameHostId()) {
      matching_prefetch = true;
      break;
    }
  }

  base::UmaHistogramBoolean(
      "PrefetchProxy.Prefetch.ExistingPrefetchWithMatchingURL",
      matching_prefetch);
}

}  // namespace content
