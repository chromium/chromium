// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_service_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/btm/btm_storage.h"
#include "content/browser/btm/btm_utils.h"
#include "content/browser/btm/persistent_repeating_timer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/btm_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

BtmRedirectCategory ClassifyRedirect(BtmDataAccessType access,
                                     bool has_user_activation) {
  using enum BtmRedirectCategory;

  switch (access) {
    case BtmDataAccessType::kUnknown:
      return has_user_activation ? kUnknownCookies_HasEngagement
                                 : kUnknownCookies_NoEngagement;
    case BtmDataAccessType::kNone:
      return has_user_activation ? kNoCookies_HasEngagement
                                 : kNoCookies_NoEngagement;
    case BtmDataAccessType::kRead:
      return has_user_activation ? kReadCookies_HasEngagement
                                 : kReadCookies_NoEngagement;
    case BtmDataAccessType::kWrite:
      return has_user_activation ? kWriteCookies_HasEngagement
                                 : kWriteCookies_NoEngagement;
    case BtmDataAccessType::kReadWrite:
      return has_user_activation ? kReadWriteCookies_HasEngagement
                                 : kReadWriteCookies_NoEngagement;
  }
}

inline void UmaHistogramBounceCategory(BtmRedirectCategory category,
                                       BtmCookieMode mode,
                                       BtmRedirectType type) {
  const std::string histogram_name =
      base::StrCat({"Privacy.DIPS.BounceCategory", GetHistogramPiece(type),
                    GetHistogramSuffix(mode)});
  base::UmaHistogramEnumeration(histogram_name, category);
}

inline void UmaHistogramDeletionLatency(base::Time deletion_start) {
  base::UmaHistogramLongTimes100("Privacy.DIPS.DeletionLatency2",
                                 base::Time::Now() - deletion_start);
}

inline void UmaHistogramClearedSitesCount(BtmCookieMode mode, int size) {
  base::UmaHistogramCounts1000(base::StrCat({"Privacy.DIPS.ClearedSitesCount",
                                             GetHistogramSuffix(mode)}),
                               size);
}

inline void UmaHistogramBounceDelay(base::TimeDelta sample) {
  base::UmaHistogramTimes("Privacy.DIPS.ServerBounceDelay", sample);
}

inline void UmaHistogramBounceChainDelay(base::TimeDelta sample) {
  base::UmaHistogramTimes("Privacy.DIPS.ServerBounceChainDelay", sample);
}

inline void UmaHistogramBounceStatusCode(int response_code, bool cached) {
  base::UmaHistogramSparse(cached ? "Privacy.DIPS.BounceStatusCode.Cached"
                                  : "Privacy.DIPS.BounceStatusCode.NoCache",
                           response_code);
}

inline void UmaHistogramDeletion(BtmCookieMode mode, BtmDeletionAction action) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Privacy.DIPS.Deletion", GetHistogramSuffix(mode)}),
      action);
}

inline void UmaHistogramSiteToClearDomainLength(
    std::string const& site_to_clear,
    bool is_canonical_host) {
  base::UmaHistogramSparse(
      is_canonical_host ? "Privacy.DIPS.DeletionDomainLength.Serializable"
                        : "Privacy.DIPS.DeletionDomainLength.NonCanonical",
      site_to_clear.length());
}

void RecordRedirectMetrics(const BtmRedirectInfo& redirect,
                           const BtmRedirectChainInfo& chain) {
  DCHECK(redirect.site_had_user_activation.has_value());
  DCHECK(redirect.site_had_webauthn_assertion.has_value());
  DCHECK(redirect.chain_id.has_value());
  DCHECK(redirect.chain_index.has_value());
  DCHECK_LT(redirect.chain_index.value(), chain.length);

  bool initial_site_same = (redirect.site == chain.initial_site);
  bool final_site_same = (redirect.site == chain.final_site);

  if (!chain.are_3pcs_generally_enabled) {
    ukm::builders::BTM_Redirect(redirect.redirector_source_id)
        .SetSiteHadUserActivation(redirect.site_had_user_activation.value())
        .SetSiteHadWebAuthnAssertion(
            redirect.site_had_webauthn_assertion.value())
        .SetRedirectType(static_cast<int64_t>(redirect.redirect_type))
        .SetCookieAccessType(static_cast<int64_t>(redirect.access_type))
        .SetRedirectAndInitialSiteSame(initial_site_same)
        .SetRedirectAndFinalSiteSame(final_site_same)
        .SetInitialAndFinalSitesSame(chain.initial_and_final_sites_same)
        .SetRedirectChainIndex(redirect.chain_index.value())
        .SetRedirectChainLength(chain.length)
        .SetIsPartialRedirectChain(chain.is_partial_chain)
        .SetClientBounceDelay(
            BucketizeBtmBounceDelay(redirect.client_bounce_delay))
        .SetHasStickyActivation(redirect.has_sticky_activation)
        .SetWebAuthnAssertionRequestSucceeded(
            redirect.web_authn_assertion_request_succeeded)
        .SetChainId(redirect.chain_id.value())
        .Record(ukm::UkmRecorder::Get());
  }

  // Don't record UMA metrics for same-site redirects.
  if (initial_site_same || final_site_same) {
    return;
  }

  BtmRedirectCategory category = ClassifyRedirect(
      redirect.access_type, redirect.site_had_user_activation.value());
  UmaHistogramBounceCategory(category, chain.cookie_mode.value(),
                             redirect.redirect_type);

  if (redirect.redirect_type == BtmRedirectType::kServer) {
    UmaHistogramBounceDelay(redirect.server_bounce_delay);
    UmaHistogramBounceStatusCode(redirect.response_code,
                                 redirect.was_response_cached);
  }
}

net::CookiePartitionKeyCollection CookiePartitionKeyCollectionForSites(
    const std::vector<std::string>& sites) {
  std::vector<net::CookiePartitionKey> keys;
  for (const auto& site : sites) {
    for (const auto& [scheme, port] :
         {std::make_pair("http", 80), std::make_pair("https", 443)}) {
      std::optional<url::Origin> origin =
          url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
              scheme, site, port);
      UmaHistogramSiteToClearDomainLength(site, origin.has_value());
      // The host may be non-canonical or invalid. In such a case, we ignore it,
      // since it will cause IPC deserialization issues later on.
      if (!origin.has_value()) {
        break;
      }
      for (auto ancestorChainBit :
           {net::CookiePartitionKey::AncestorChainBit::kSameSite,
            net::CookiePartitionKey::AncestorChainBit::kCrossSite}) {
        std::optional<net::CookiePartitionKey> key =
            net::CookiePartitionKey::FromStorageKeyComponents(
                net::SchemefulSite(*origin), ancestorChainBit,
                /*nonce=*/std::nullopt);
        if (key.has_value()) {
          keys.push_back(*key);
        }
      }
    }
  }
  return net::CookiePartitionKeyCollection(keys);
}

class StateClearer : public BrowsingDataRemover::Observer {
 public:
  StateClearer(const StateClearer&) = delete;
  StateClearer& operator=(const StateClearer&) = delete;

  ~StateClearer() override { remover_->RemoveObserver(this); }

  // Clears state for the sites in `sites_to_clear`. Runs `callback` once
  // clearing is complete.
  //
  // NOTE: This deletion task removing rows for `sites_to_clear` from the
  // BtmStorage backend relies on the assumption that rows flagged as BTM
  // eligible don't have user activation time values. So even though 'remover'
  // will only clear the storage timestamps, that's sufficient to delete the
  // entire row.
  static void DeleteState(BrowsingDataRemover* remover,
                          std::vector<std::string> sites_to_clear,
                          BrowsingDataRemover::DataType remove_mask,
                          base::OnceClosure callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // This filter will match unpartitioned cookies and storage, as well as
    // storage (but not cookies) that is partitioned under tracking domains.
    std::unique_ptr<BrowsingDataFilterBuilder> filter =
        BrowsingDataFilterBuilder::Create(
            BrowsingDataFilterBuilder::Mode::kDelete);
    for (const auto& site : sites_to_clear) {
      filter->AddRegisterableDomain(site);
    }
    // Don't delete CHIPS partitioned under non-tracking sites.
    filter->SetCookiePartitionKeyCollection(
        net::CookiePartitionKeyCollection());

    // This filter will match cookies partitioned under tracking domains.
    std::unique_ptr<BrowsingDataFilterBuilder> partitioned_cookie_filter =
        BrowsingDataFilterBuilder::Create(
            BrowsingDataFilterBuilder::Mode::kPreserve);
    partitioned_cookie_filter->SetCookiePartitionKeyCollection(
        CookiePartitionKeyCollectionForSites(sites_to_clear));
    partitioned_cookie_filter->SetPartitionedCookiesOnly(true);
    // We don't add any domains to this filter, so with mode=kPreserve it will
    // delete everything partitioned under the sites.

    // StateClearer manages its own lifetime and deletes itself when finished.
    StateClearer* clearer =
        new StateClearer(remover, /*callback_count=*/2, std::move(callback));

    // Don't delete Privacy Sandbox data - see crbug.com/41488981.
    remove_mask &= ~BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX;
    remover->RemoveWithFilterAndReply(
        base::Time::Min(), base::Time::Max(),
        remove_mask | BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
        std::move(filter), clearer);
    remover->RemoveWithFilterAndReply(
        base::Time::Min(), base::Time::Max(),
        BrowsingDataRemover::DATA_TYPE_COOKIES,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
        std::move(partitioned_cookie_filter), clearer);
  }

 private:
  // StateClearer will run `callback` and delete itself after
  // OnBrowsingDataRemoverDone() is called `callback_count` times.
  StateClearer(BrowsingDataRemover* remover,
               int callback_count,
               base::OnceClosure callback)
      : remover_(remover),
        deletion_start_(base::Time::Now()),
        expected_callback_count_(callback_count),
        callback_(std::move(callback)) {
    remover_->AddObserver(this);
  }

  // BrowsingDataRemover::Observer overrides:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    if (++callback_count_ == expected_callback_count_) {
      UmaHistogramDeletionLatency(deletion_start_);
      std::move(callback_).Run();
      delete this;  // Matches the new in DeleteState()
    }
  }

  raw_ptr<BrowsingDataRemover> remover_;
  const base::Time deletion_start_;
  const int expected_callback_count_;
  int callback_count_ = 0;
  base::OnceClosure callback_;
};

class DipsTimerStorage : public PersistentRepeatingTimer::Storage {
 public:
  explicit DipsTimerStorage(base::SequenceBound<BtmStorage>* dips_storage);
  ~DipsTimerStorage() override;

  // Reads the timestamp from the DIPS DB.
  void GetLastFired(TimeCallback callback) const override {
    dips_storage_->AsyncCall(&BtmStorage::GetTimerLastFired)
        .Then(std::move(callback));
  }
  // Write the timestamp to the DIPS DB.
  void SetLastFired(base::Time time) override {
    dips_storage_->AsyncCall(base::IgnoreResult(&BtmStorage::SetTimerLastFired))
        .WithArgs(time);
  }

 private:
  raw_ref<base::SequenceBound<BtmStorage>> dips_storage_;
};

DipsTimerStorage::DipsTimerStorage(
    base::SequenceBound<BtmStorage>* dips_storage)
    : dips_storage_(CHECK_DEREF(dips_storage)) {}

DipsTimerStorage::~DipsTimerStorage() = default;

}  // namespace

// static
BtmService* BtmService::Get(BrowserContext* context) {
  return BtmServiceImpl::Get(context);
}

BtmServiceImpl::BtmServiceImpl(base::PassKey<BrowserContextImpl>,
                               BrowserContext* context)
    : browser_context_(context) {
  DCHECK(base::FeatureList::IsEnabled(features::kBtm));
  base::FilePath btm_path = GetBtmFilePath(browser_context_);
  // This feature explicitly uses in-memory storage on WebEngine on Fuchsia to
  // avoid consuming too much storage space. WebEngine has only 2MB of storage
  // for the user data directory.
  const bool use_in_memory_db =
#if BUILDFLAG(IS_FUCHSIA) && defined(IS_WEB_ENGINE)
      true;
#else
      browser_context_->IsOffTheRecord();
#endif
  storage_ =
      use_in_memory_db
          ? base::SequenceBound<BtmStorage>(CreateTaskRunner(), std::nullopt)
          : base::SequenceBound<BtmStorage>(
                CreateTaskRunnerForResource(btm_path), btm_path);
#if BUILDFLAG(IS_FUCHSIA) && defined(IS_WEB_ENGINE)
  // WebEngine on Fuchsia has a limited amount of storage, so we don't want to
  // keep around any data from previous sessions before the change was made to
  // always use an in-memory database.
  BtmStorage::DeleteDatabaseFiles(btm_path,
                                  fuchsia_cleanup_loop_.QuitClosure());
#endif

  repeating_timer_ = CreateTimer();
  repeating_timer_->Start();
}

std::unique_ptr<PersistentRepeatingTimer> BtmServiceImpl::CreateTimer() {
  CHECK(!storage_.is_null());
  // base::Unretained(this) is safe here since the timer that is created has the
  // same lifetime as this service.
  return std::make_unique<PersistentRepeatingTimer>(
      std::make_unique<DipsTimerStorage>(&storage_),
      features::kBtmTimerDelay.Get(),
      base::BindRepeating(&BtmServiceImpl::OnTimerFired,
                          base::Unretained(this)));
}

BtmServiceImpl::~BtmServiceImpl() {
  // Some UserData may interact with `this` during their destruction. Delete
  // them now, before it's too late. If we don't delete them manually,
  // ~SupportsUserData() will, but `this` will be invalid at that time.
  //
  // Note that we can't put this call in ~BtmService() either, even though
  // BtmService is the class that directly inherits from SupportsUserData.
  // Because when ~BtmService() is called, it's undefined behavior to call
  // pure virtual functions like BtmService::RemoveObserver().
  ClearAllUserData();
}

// static
BtmServiceImpl* BtmServiceImpl::Get(BrowserContext* context) {
  return BrowserContextImpl::From(context)->GetBtmService();
}

scoped_refptr<base::SequencedTaskRunner> BtmServiceImpl::CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND});
}

scoped_refptr<base::SequencedTaskRunner>
BtmServiceImpl::CreateTaskRunnerForResource(const base::FilePath& path) {
  return base::ThreadPool::CreateSequencedTaskRunnerForResource(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::PREFER_BACKGROUND},
      path);
}

BtmCookieMode BtmServiceImpl::GetCookieMode() const {
  return GetBtmCookieMode(browser_context_->IsOffTheRecord());
}

void BtmServiceImpl::RemoveEvents(const base::Time& delete_begin,
                                  const base::Time& delete_end,
                                  network::mojom::ClearDataFilterPtr filter,
                                  BtmEventRemovalType type) {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&BtmStorage::RemoveEvents)
      .WithArgs(delete_begin, delete_end, std::move(filter), type);
}

void BtmServiceImpl::HandleRedirectChain(
    std::vector<BtmRedirectInfoPtr> redirects,
    BtmRedirectChainInfoPtr chain,
    StatefulBounceCallback stateful_bounce_callback) {
  DCHECK_LE(redirects.size(), chain->length);

  if (redirects.empty()) {
    DCHECK(!chain->is_partial_chain);
    for (auto& observer : observers_) {
      observer.OnChainHandled(redirects, chain);
    }
    return;
  }

  if (!chain->are_3pcs_generally_enabled &&
      chain->initial_source_id != ukm::kInvalidSourceId) {
    ukm::builders::BTM_ChainBegin(chain->initial_source_id)
        .SetChainId(chain->chain_id)
        .SetInitialAndFinalSitesSame(chain->initial_and_final_sites_same)
        .Record(ukm::UkmRecorder::Get());
  }

  if (!chain->are_3pcs_generally_enabled &&
      chain->final_source_id != ukm::kInvalidSourceId) {
    ukm::builders::BTM_ChainEnd(chain->final_source_id)
        .SetChainId(chain->chain_id)
        .SetInitialAndFinalSitesSame(chain->initial_and_final_sites_same)
        .Record(ukm::UkmRecorder::Get());
  }

  std::set<std::string> redirect_sites;
  base::TimeDelta total_server_bounce_delay;
  for (const auto& redirect : redirects) {
    if (redirect->redirect_type == BtmRedirectType::kServer) {
      total_server_bounce_delay += redirect->server_bounce_delay;
    }
    redirect_sites.insert(GetSiteForBtm(redirect->redirector_url));
  }
  UmaHistogramBounceChainDelay(total_server_bounce_delay);

  chain->cookie_mode = GetCookieMode();
  storage_.AsyncCall(&BtmStorage::FilterSitesWithProtectiveEvent)
      .WithArgs(redirect_sites)
      .Then(base::BindOnce(&BtmServiceImpl::HandleRedirects,
                           weak_factory_.GetWeakPtr(), std::move(redirects),
                           std::move(chain), stateful_bounce_callback));
}

void BtmServiceImpl::HandleRedirects(
    std::vector<BtmRedirectInfoPtr> redirects,
    BtmRedirectChainInfoPtr chain,
    StatefulBounceCallback stateful_bounce_callback,
    std::pair<std::set<std::string>, std::set<std::string>>
        sites_with_protective_events) {
  const auto& [sites_with_user_activation, sites_with_webauthn_assertion] =
      sites_with_protective_events;
  for (size_t index = 0; index < redirects.size(); index++) {
    auto& redirect = *redirects[index];

    DCHECK(!redirect.site_had_user_activation.has_value());
    redirect.site_had_user_activation =
        sites_with_user_activation.contains(redirect.site);
    DCHECK(!redirect.site_had_webauthn_assertion.has_value());
    redirect.site_had_webauthn_assertion =
        sites_with_webauthn_assertion.contains(redirect.site);
    DCHECK(!redirect.chain_id.has_value());
    redirect.chain_id = chain->chain_id;
    // If the chain was too long, some redirects may have been trimmed already,
    // which would make `index` not the "true" index of the redirect in the
    // whole chain. `chain->length` is accurate though. `chain->length -
    // redirects.size()` is then the number of trimmed redirects; so add that to
    // `index` to get the "true" index to report in our metrics.
    DCHECK(!redirect.chain_index.has_value());
    redirect.chain_index = chain->length - redirects.size() + index;

    RecordRedirectMetrics(redirect, *chain);

    bool initial_site_same = (redirect.site == chain->initial_site);
    bool final_site_same = (redirect.site == chain->final_site);

    if (initial_site_same || final_site_same) {
      continue;
    }
    if (redirect.access_type == BtmDataAccessType::kUnknown) {
      continue;
    }

    RecordBounce(stateful_bounce_callback, redirect, *chain);
  }

  // All redirects handled.
  if (!chain->is_partial_chain) {
    for (auto& observer : observers_) {
      observer.OnChainHandled(redirects, chain);
    }
  }
}

void BtmServiceImpl::RecordBounce(
    StatefulBounceCallback stateful_bounce_callback,
    const BtmRedirectInfo& redirect,
    const BtmRedirectChainInfo& chain) {
  const GURL& url = redirect.redirector_url;
  bool stateful = redirect.access_type > BtmDataAccessType::kRead;

  // If the bounced URL has a 3PC exception when embedded under the initial or
  // final URL in the redirect, then clear the tracking site from the BTM
  // database to avoid deleting its storage. The exception overrides any bounces
  // from non-excepted sites.
  if (redirect.has_3pc_exception.value()) {
    // Check whether the site would have hypothetically been cleared.
    bool would_be_cleared;
    // TODO(crbug.com/430921459): Refactor killswitch behavior into the
    // top-level feature so there's no need to maintain multiple triggering
    // actions.
    switch (features::kBtmTriggeringAction.Get()) {
      case BtmTriggeringAction::kNone: {
        would_be_cleared = false;
        break;
      }
      case BtmTriggeringAction::kBounce: {
        would_be_cleared = true;
        break;
      }
    }
    if (!chain.are_3pcs_generally_enabled && would_be_cleared) {
      // TODO(crbug.com/40268849): Investigate and fix the presence of empty
      // site(s) in the `site_to_clear` list. Once this is fixed remove this
      // escape.
      if (url.is_empty()) {
        UmaHistogramDeletion(GetCookieMode(), BtmDeletionAction::kIgnored);
        return;
      }
      UmaHistogramDeletion(GetCookieMode(), BtmDeletionAction::kExcepted);
    }

    const std::set<std::string> site_to_clear{GetSiteForBtm(url)};
    // Don't clear the row if the tracker has history indicating that we
    // should preserve that context for future bounces.
    storage_.AsyncCall(&BtmStorage::RemoveRowsWithoutProtectiveEvent)
        .WithArgs(site_to_clear);

    return;
  }

  // If the bounce is stateful and not excepted by cookie settings, run the
  // callback.
  if (stateful) {
    stateful_bounce_callback.Run(chain.final_url);
  }

  // Record the bounce at the storage layer.
  storage_.AsyncCall(&BtmStorage::RecordBounce).WithArgs(url, redirect.time);
}

// static
void BtmServiceImpl::RecordRedirectMetricsForTesting(
    const BtmRedirectInfo& redirect,
    const BtmRedirectChainInfo& chain) {
  RecordRedirectMetrics(redirect, chain);
}

void BtmServiceImpl::OnTimerFired() {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&BtmStorage::GetSitesToClear)
      .WithArgs(std::nullopt)
      .Then(base::BindOnce(&BtmServiceImpl::DeleteBtmEligibleState,
                           weak_factory_.GetWeakPtr(), base::DoNothing()));
}

void BtmServiceImpl::DeleteEligibleSitesImmediately(
    DeletedSitesCallback callback) {
  // Storage init should be finished by now, so no need to delay until then.
  storage_.AsyncCall(&BtmStorage::GetSitesToClear)
      .WithArgs(base::Seconds(0))
      .Then(base::BindOnce(&BtmServiceImpl::DeleteBtmEligibleState,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BtmServiceImpl::DeleteBtmEligibleState(
    DeletedSitesCallback callback,
    std::vector<std::string> sites_to_clear) {
  // Do not clear sites from currently open tabs.
  for (const std::pair<std::string, int> site_ctr : open_sites_) {
    CHECK(site_ctr.second > 0);
    std::erase(sites_to_clear, site_ctr.first);
  }

  std::vector<std::string> filtered_sites_to_clear;
  for (const auto& site : sites_to_clear) {
    // TODO(crbug.com/40268849): Investigate and fix the presence of empty
    // site(s) in the `site_to_clear` list. Once this is fixed remove this loop
    // escape.
    if (site.empty()) {
      UmaHistogramDeletion(GetCookieMode(), BtmDeletionAction::kIgnored);
      continue;
    }
    UmaHistogramDeletion(GetCookieMode(), BtmDeletionAction::kEnforced);

    const ukm::SourceId source_id = ukm::UkmRecorder::GetSourceIdForDipsSite(
        base::PassKey<BtmServiceImpl>(), site);
    ukm::builders::DIPS_Deletion(source_id)
        // These settings are checked at bounce time, before logging the bounce.
        // At this time, we guarantee that 3PC are blocked and this site is not
        // excepted (provided the user hasn't changed their settings in the
        // meantime).
        .SetShouldBlockThirdPartyCookies(true)
        .SetHasCookieException(false)
        .SetIsDeletionEnabled(true)
        .Record(ukm::UkmRecorder::Get());

    filtered_sites_to_clear.push_back(site);
  }

  UmaHistogramClearedSitesCount(GetCookieMode(), sites_to_clear.size());
  base::OnceClosure finish_callback = base::BindOnce(
      std::move(callback), std::vector<std::string>(filtered_sites_to_clear));
  if (filtered_sites_to_clear.empty()) {
    std::move(finish_callback).Run();
    return;
  }

  // Perform state deletion on the filtered list of sites.
  RunDeletionTaskOnUIThread(std::move(filtered_sites_to_clear),
                            std::move(finish_callback));
}

void BtmServiceImpl::RunDeletionTaskOnUIThread(std::vector<std::string> sites,
                                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  uint64_t remove_mask = GetContentClient()->browser()->GetBtmRemoveMask();

  StateClearer::DeleteState(browser_context_->GetBrowsingDataRemover(),
                            std::move(sites), remove_mask, std::move(callback));
}

void BtmServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BtmServiceImpl::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BtmServiceImpl::RecordUserActivationForTesting(const GURL& url) {
  storage_.AsyncCall(&BtmStorage::RecordUserActivation)
      .WithArgs(url, base::Time::Now());
}

void BtmServiceImpl::DidSiteHaveUserActivationSince(
    const GURL& url,
    base::Time bound,
    CheckUserActivationCallback callback) const {
  storage_.AsyncCall(&BtmStorage::DidSiteHaveUserActivationSince)
      .WithArgs(url, bound)
      .Then(std::move(callback));
}

void BtmServiceImpl::RecordBrowserSignIn(std::string_view domain) {
  storage()
      ->AsyncCall(&BtmStorage::RecordUserActivation)
      .WithArgs(url::SchemeHostPort("http", domain, 80).GetURL(),
                base::Time::Now());
}

void BtmServiceImpl::NotifyStatefulBounce(WebContents* web_contents) {
  for (auto& observer : observers_) {
    observer.OnStatefulBounce(web_contents);
  }
}

}  // namespace content
