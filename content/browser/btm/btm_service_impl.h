// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BTM_BTM_SERVICE_IMPL_H_

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/browser/btm/btm_storage.h"
#include "content/browser/btm/btm_utils.h"
#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/btm_service.h"

namespace content {

class BrowserContext;
class BrowserContextImpl;
class PersistentRepeatingTimer;

// `BtmServiceImpl` is intentionally *not* exposed in the Content API — we only
// want other `//content` code (such as the BTM implementation) to access it, as
// `BtmServiceImpl` is an implementation detail that `//content` embedders
// shouldn't know about.
class CONTENT_EXPORT BtmServiceImpl : public BtmService {
 public:
  using StatefulBounceCallback = base::RepeatingCallback<void(const GURL&)>;

  using RecordBounceCallback =
      base::RepeatingCallback<void(const BtmRedirectInfo& redirect,
                                   const BtmRedirectChainInfo& chain)>;

  BtmServiceImpl(base::PassKey<BrowserContextImpl>, BrowserContext* context);
  ~BtmServiceImpl() override;

  static BtmServiceImpl* Get(BrowserContext* context);

  base::SequenceBound<BtmStorage>* storage() { return &storage_; }

  void RecordBounceForTesting(const BtmRedirectInfo& redirect,
                              const BtmRedirectChainInfo& chain,
                              StatefulBounceCallback stateful_bounce_callback) {
    RecordBounce(stateful_bounce_callback, redirect, chain);
  }

  BtmCookieMode GetCookieMode() const;

  void RemoveEvents(const base::Time& delete_begin,
                    const base::Time& delete_end,
                    network::mojom::ClearDataFilterPtr filter,
                    const BtmEventRemovalType type);

  // This allows for deletion of state for sites deemed eligible when evaluated
  // with no grace period.
  void DeleteEligibleSitesImmediately(DeletedSitesCallback callback) override;

  // Processes a redirect chain to identify and record bounces. The main
  // entrypoint for the BTM feature to act on navigations.
  void HandleRedirectChain(std::vector<BtmRedirectInfoPtr> redirects,
                           BtmRedirectChainInfoPtr chain,
                           StatefulBounceCallback stateful_bounce_callback);

  void RecordUserActivationForTesting(const GURL& url) override;

  void DidSiteHaveUserActivationSince(
      const GURL& url,
      base::Time bound,
      CheckUserActivationCallback callback) const override;

  // This allows unit-testing the metrics recording without instantiating
  // BtmService. Just calls the internal RecordRedirectMetrics function.
  static void RecordRedirectMetricsForTesting(
      const BtmRedirectInfo& redirect,
      const BtmRedirectChainInfo& chain);

  void SetStorageClockForTesting(base::Clock* clock) {
    DCHECK(storage_);
    storage_.AsyncCall(&BtmStorage::SetClockForTesting).WithArgs(clock);
  }

  void OnTimerFiredForTesting() { OnTimerFired(); }

#if BUILDFLAG(IS_FUCHSIA) && defined(IS_WEB_ENGINE)
  void WaitForFuchsiaCleanupForTesting() { fuchsia_cleanup_loop_.Run(); }
#endif

  void AddObserver(Observer* observer) override;
  void RemoveObserver(const Observer* observer) override;

  inline void AddOpenSite(const std::string& site) { ++open_sites_[site]; }

  void RemoveOpenSite(const std::string& site) {
    auto it = open_sites_.find(site);
    bool site_found = it != open_sites_.end();
    CHECK(site_found);
    if (site_found) {
      --it->second;
      if (it->second == 0) {
        open_sites_.erase(it);
      }
    }
  }

  // Notify Observers that a stateful bounce took place in `web_contents`.
  void NotifyStatefulBounce(WebContents* web_contents);

 private:
  std::unique_ptr<PersistentRepeatingTimer> CreateTimer();

  // Processes redirects to identify and record bounces.
  void HandleRedirects(std::vector<BtmRedirectInfoPtr> redirects,
                       BtmRedirectChainInfoPtr chain,
                       StatefulBounceCallback stateful_bounce_callback,
                       std::pair<std::set<std::string>, std::set<std::string>>
                           sites_with_protective_events);
  void RecordBounce(StatefulBounceCallback stateful_bounce_callback,
                    const BtmRedirectInfo& redirect,
                    const BtmRedirectChainInfo& chain);

  scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner();
  scoped_refptr<base::SequencedTaskRunner> CreateTaskRunnerForResource(
      const base::FilePath& path);

  void OnTimerFired();
  void DeleteBtmEligibleState(DeletedSitesCallback callback,
                              std::vector<std::string> sites_to_clear);
  void RunDeletionTaskOnUIThread(std::vector<std::string> sites_to_clear,
                                 base::OnceClosure callback);

  // BtmService overrides:
  void RecordBrowserSignIn(std::string_view domain) override;

  raw_ptr<BrowserContext> browser_context_;
  // The persisted timer controlling how often incidental state is cleared.
  // This timer is null if the BTM feature isn't enabled with a valid TimeDelta
  // given for its `timer_delay` parameter.
  // See base/time/time_delta_from_string.h for how that param should be given.
  std::unique_ptr<PersistentRepeatingTimer> repeating_timer_;
  base::SequenceBound<BtmStorage> storage_;
  base::ObserverList<Observer> observers_;

  // A map from site (eTLD+1) to the number of tabs that have that site open.
  // Used to avoid clearing state for sites that are currently in use.
  std::map<std::string, int> open_sites_;

#if BUILDFLAG(IS_FUCHSIA) && defined(IS_WEB_ENGINE)
  // If running on WebEngine on Fuchsia, any existing BTM database file is
  // asynchronously deleted. This RunLoop allows tests to wait for the
  // deletion to complete.
  //
  // TODO: crbug.com/434764000 - delete this once we are confident any leftover
  // database files have been removed on WebEngine on Fuchsia.
  base::RunLoop fuchsia_cleanup_loop_;
#endif

  base::WeakPtrFactory<BtmServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_SERVICE_IMPL_H_
