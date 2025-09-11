// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_BOUNCE_DETECTOR_H_
#define CONTENT_BROWSER_BTM_BTM_BOUNCE_DETECTOR_H_

#include <memory>
#include <string>
#include <variant>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_utils.h"
#include "content/browser/btm/cookie_access_filter.h"
#include "content/common/content_export.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/shared_worker_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace content {

class WebContents;

using BtmRedirectChainHandler =
    base::RepeatingCallback<void(std::vector<BtmRedirectInfoPtr>,
                                 BtmRedirectChainInfoPtr)>;
using BtmIssueHandler =
    base::RepeatingCallback<void(std::set<std::string> sites)>;
using Btm3PcSettingsCallback = base::RepeatingCallback<bool()>;
using BtmIssueReportingCallback =
    base::RepeatingCallback<void(const std::set<std::string>& sites)>;

// ClientBounceDetectionState is owned by the BtmBounceDetector and stores
// data needed to detect stateful client-side redirects.
class ClientBounceDetectionState {
 public:
  ClientBounceDetectionState(GURL url,
                             std::string site,
                             base::TimeTicks load_time);
  ClientBounceDetectionState(const ClientBounceDetectionState& other);
  ~ClientBounceDetectionState();

  // The NavigationHandle's previously committed URL at the time the navigation
  // finishes and commits.
  GURL previous_url;
  std::string current_site;
  base::TimeTicks page_load_time;
  std::optional<base::Time> last_activation_time;
  std::optional<base::Time> last_storage_time;
  std::optional<base::Time> last_successful_web_authn_assertion_time;
  BtmDataAccessType site_data_access_type = BtmDataAccessType::kUnknown;
};

// Either the URL navigated away from (starting a new chain), or the client-side
// redirect connecting the navigation to the currently-committed chain.
// TODO: crbug.com/324573484 - rename to remove association with DIPS.
using BtmNavigationStart =
    std::variant<std::pair<GURL, ukm::SourceId>, BtmRedirectInfoPtr>;

// In case of a client-side redirect loop, we need to impose a limit on the
// stored redirect chain to avoid boundless memory use. Past this limit,
// redirects are trimmed from the front of the list.
constexpr size_t kBtmRedirectChainMax = 1000;

// When checking the history of the current tab for sites following the
// first-party site, this is the maximum number of navigation entries to check.
inline constexpr int kAllSitesFollowingFirstPartyLookbackLength = 10;

// A redirect-chain-in-progress. It grows by calls to Append() and restarts by
// calls to EndChain(). Runs a `BtmRedirectChainHandler` when the chain is
// complete.
//
// TODO: crbug.com/324573484 - rename to remove association with BTM.
class CONTENT_EXPORT BtmRedirectContext {
 public:
  BtmRedirectContext(BtmRedirectChainHandler handler,
                     BtmIssueHandler issue_handler,
                     Btm3PcSettingsCallback are_3pcs_generally_enabled_callback,
                     const GURL& initial_url,
                     ukm::SourceId initial_source_id,
                     size_t redirect_prefix_count);
  ~BtmRedirectContext();

  // Immediately calls the `BtmRedirectChainHandler` for the uncommitted
  // navigation. It will take into account the length and initial URL of the
  // current chain (without modifying it).
  void HandleUncommitted(BtmNavigationStart navigation_start,
                         std::vector<BtmRedirectInfoPtr> server_redirects);

  // Either calls for termination of the in-progress redirect chain, with a
  // start of a new one, or extends it, according to the value of
  // `navigation_start`.
  void AppendCommitted(BtmNavigationStart navigation_start,
                       std::vector<BtmRedirectInfoPtr> server_redirects,
                       const GURL& final_url,
                       ukm::SourceId final_source_id,
                       bool current_page_has_interaction);

  // Terminates the in-progress redirect chain, ending it with `final_url`, and
  // passing it to the `BtmRedirectChainHandler` iff the chain is valid. It
  // also starts a fresh redirect chain with `final_url` and `final_source_id`
  // whilst clearing the state of the terminated chain. NOTE: A chain is valid
  // if it has a non-empty `initial_url_`.
  void EndChain(GURL final_url,
                ukm::SourceId final_source_id,
                bool current_page_has_interaction);

  // Reports a BTM issue to the inspector (e.g., DevTools).
  void ReportIssue(const GURL& final_url);

  // Attempts to attribute a late cookie access `op` to a recent redirect with
  // URL `url`. A late cookie access is a navigational cookie access for which
  // the notification arrives after the navigation has finished. Returns true if
  // the cookie access was attributed to a redirect, and false otherwise.
  [[nodiscard]] bool AddLateCookieAccess(const GURL& url, CookieOperation op);

  size_t size() const { return redirects_.size(); }

  const GURL& GetInitialURLForTesting() const { return initial_url_; }

  void SetRedirectChainHandlerForTesting(BtmRedirectChainHandler handler) {
    handler_ = handler;
  }

  // Returns the total number of redirects in the chain, including any that
  // preceded this chain that should count toward this chain's length.
  size_t GetRedirectChainLength() const {
    return redirects_.size() + redirect_prefix_count_;
  }

  const BtmRedirectInfo& operator[](size_t index) const {
    return *redirects_.at(index);
  }

  std::optional<std::pair<size_t, BtmRedirectInfo*>> GetRedirectInfoFromChain(
      const std::string& site) const;

  // Return whether `site` had a user activation or authentication interaction
  // in the current redirect context.
  bool SiteHadUserActivationOrAuthn(const std::string& site) const;

  // Return all sites that had an interaction in the current redirect context.
  std::set<std::string> AllSitesWithUserActivationOrAuthn() const;

  // Returns the server redirects from the last navigation. Note that due to
  // limitations in C++ the BtmRedirectInfo objects are unavoidably mutable.
  // Clients MUST NOT modify them.
  base::span<const BtmRedirectInfoPtr>
  GetServerRedirectsSinceLastPrimaryPageChange() const;

 private:
  void AppendClientRedirect(BtmRedirectInfoPtr client_redirect);
  void AppendServerRedirects(std::vector<BtmRedirectInfoPtr> server_redirects);
  // Evicts the first redirect from the front of the in-progress redirect chain
  // if it exceeds its max allowed length of `kBtmRedirectChainMax`, and passes
  // any evicted redirects as partial chains to the `BtmRedirectChainHandler`.
  void MaybeTrimAndHandlePartialRedirectChain();

  BtmRedirectChainHandler handler_;
  BtmIssueHandler issue_handler_;
  Btm3PcSettingsCallback are_3pcs_generally_enabled_callback_;
  // Represents the start of a chain and also indicates the presence of a valid
  // chain.
  GURL initial_url_;
  ukm::SourceId initial_source_id_;
  // Whether the initial_url_ had a user activation or web authentication
  // interaction while loaded.
  bool initial_url_had_interaction_;

  // TODO(amaliev): Make redirects_ a circular queue to handle the memory bound
  // more gracefully.
  std::vector<BtmRedirectInfoPtr> redirects_;
  std::set<std::string> redirectors_;
  // The number of redirects preceding this chain, that should be counted toward
  // this chain's total length. Includes both committed redirects (for an
  // uncommitted chain) and trimmed redirects. (Redirects may be trimmed from
  // the front of an ongoing chain for memory management.)
  size_t redirect_prefix_count_ = 0;
};

// A simplified interface to WebContents and BtmServiceImpl that can be faked
// in tests. Needed to allow unit testing BtmBounceDetector.
//
// TODO: crbug.com/324573484 - rename to remove association with BTM.
class CONTENT_EXPORT BtmBounceDetectorDelegate {
 public:
  virtual ~BtmBounceDetectorDelegate();
  virtual GURL GetLastCommittedURL() const = 0;
  virtual ukm::SourceId GetLastCommittedSourceId() const = 0;
  virtual void HandleRedirectChain(std::vector<BtmRedirectInfoPtr> redirects,
                                   BtmRedirectChainInfoPtr chain) = 0;
  // Report `sites` as redirectors to the inspector (e.g., DevTools).
  virtual void ReportRedirectors(std::set<std::string> sites) = 0;
  virtual bool Are3PcsGenerallyEnabled() const = 0;
};

// ServerBounceDetectionState gets attached to NavigationHandle (which is a
// SupportsUserData subclass) to store data needed to detect stateful
// server-side redirects.
class CONTENT_EXPORT ServerBounceDetectionState
    : public NavigationHandleUserData<ServerBounceDetectionState> {
 public:
  ServerBounceDetectionState();
  ~ServerBounceDetectionState() override;

  struct ServerRedirectData {
    const int http_response_code;
    const base::TimeDelta bounce_delay;
    const bool was_response_cached;
    const GURL destination_url;
  };

  // A BtmRedirectInfoPtr if the navigation started with a client redirect; a
  // UrlAndSourceId otherwise.
  BtmNavigationStart navigation_start;
  CookieAccessFilter filter;
  std::vector<ServerRedirectData> server_redirects;
  std::vector<ukm::SourceId> server_redirect_source_ids;
  base::TimeTicks last_server_redirect;

 private:
  explicit ServerBounceDetectionState(NavigationHandle& navigation_handle);

  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

// A simplified interface to `NavigationHandle` that can be faked in tests.
//
// TODO: crbug.com/324573484 - Rename to remove association with BTM.
// TODO: crbug.com/381687258 - Remove in favor of using `NavigationSimulator` in
// tests.
class CONTENT_EXPORT BtmNavigationHandle {
 public:
  virtual ~BtmNavigationHandle();

  // See NavigationHandle for an explanation of these methods:
  const GURL& GetURL() const { return GetRedirectChain().back(); }
  virtual ukm::SourceId GetNextPageUkmSourceId() = 0;
  virtual const GURL& GetPreviousPrimaryMainFrameURL() const = 0;
  virtual bool HasCommitted() const = 0;
  virtual const std::vector<GURL>& GetRedirectChain() const = 0;
  virtual bool WasResponseCached() = 0;
  // Get the HTTP response code from the navigation.
  virtual int GetHTTPResponseCode() = 0;
  // This method has one important (simplifying) change from
  // NavigationHandle::HasUserGesture(): it returns true if the
  // navigation was not renderer-initiated.
  virtual bool HasUserGesture() const = 0;
  //  This method doesn't have a direct equivalent in NavigationHandle,
  //  as it relies on GetInitiatorOrigin(), but returns what is effectively a
  //  base URL. Also, this returns `about:blank` if the initiator origin is
  //  unspecified or opaque.
  virtual const GURL GetInitiator() const = 0;

  // Get a SourceId of type REDIRECT_ID for the index'th URL in the redirect
  // chain.
  ukm::SourceId GetRedirectSourceId(size_t index);
  // Create a ukm::SourceId of type REDIRECT_ID for the given redirector URL.
  ukm::SourceId MakeRedirectSourceId(const GURL& url) const;
  // Calls ServerBounceDetectionState::GetOrCreateForNavigationHandle(). We
  // declare this instead of making BtmNavigationHandle a subclass of
  // SupportsUserData, because ServerBounceDetectionState inherits from
  // NavigationHandleUserData, whose helper functions only work with actual
  // NavigationHandle, not any SupportsUserData.
  virtual ServerBounceDetectionState* GetServerState() = 0;
};

// Detects client- and server-side bounces and handles them (currently by
// collecting metrics and storing them in the BtmDatabase).
//
// This class has a communication loop with `RedirectChainDetector`.
// `RedirectChainDetector` owns this class and calls it directly; this class
// then uses the `BtmBounceDetectorDelegate` interface, which
// `RedirectChainDetector` implements, to communicate back to the owning
// `RedirectChainDetector` instance.
//
// TODO: crbug.com/324573484 - rename this to avoid confusion with
// `RedirectChainDetector` and remove its association with BTM.
class CONTENT_EXPORT BtmBounceDetector {
 public:
  explicit BtmBounceDetector(BtmBounceDetectorDelegate* delegate,
                             const base::TickClock* tick_clock,
                             const base::Clock* clock);
  ~BtmBounceDetector();
  BtmBounceDetector(const BtmBounceDetector&) = delete;
  BtmBounceDetector& operator=(const BtmBounceDetector&) = delete;

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  const base::Clock* GetClock() { return clock_.get(); }
  // The following methods are based on WebContentsObserver, simplified.
  void DidStartNavigation(BtmNavigationHandle* navigation_handle);
  void DidRedirectNavigation(BtmNavigationHandle* navigation_handle);
  void OnClientSiteDataAccessed(const GURL& url, CookieOperation op);
  // Note: `navigation_handle` may be null if this server cookie access is
  // associated with a document rather than a navigation.
  void OnServerCookiesAccessed(BtmNavigationHandle* navigation_handle,
                               const GURL& url,
                               CookieOperation op);
  void DidFinishNavigation(BtmNavigationHandle* navigation_handle);
  // Only records a new user activation event once per
  // |kTimestampUpdateInterval| for a given page.
  void OnUserActivation();
  // Only records a new Web authn assertion event once per
  // |kTimestampUpdateInterval| for a given page.
  void WebAuthnAssertionRequestSucceeded();
  // Makes a call to process the current chain before its state is destroyed by
  // the tab closure.
  void BeforeDestruction();
  // Use the passed handler instead of
  // BtmBounceDetectorDelegate::HandleRedirect().
  void SetRedirectChainHandlerForTesting(BtmRedirectChainHandler handler) {
    committed_redirect_context_.SetRedirectChainHandlerForTesting(handler);
  }
  // Returns state for the in-progress redirect chain.
  const BtmRedirectContext& CommittedRedirectContext() const {
    return committed_redirect_context_;
  }

  // Attempts to attribute the late cookie access notification `op` to a recent
  // redirect by `url` in the current chain. A "late" cookie access is a
  // navigational cookie access that is reported after the navigation has
  // finished. Returns true if the access was attributed to a redirect, and
  // false otherwise.
  [[nodiscard]] bool AddLateCookieAccess(GURL url, CookieOperation op) {
    bool was_late = committed_redirect_context_.AddLateCookieAccess(url, op);
    if (was_late) {
      OnServerCookiesAccessed(/*navigation_handle=*/nullptr, url, op);
    }
    return was_late;
  }

  // Makes a call to process the current chain on
  // `client_bounce_detection_timer_`'s timeout.
  void OnClientBounceDetectionTimeout();

 private:
  // Whether or not the `last_time` timestamp should be updated yet. This is
  // used to enforce throttling of timestamp updates, reducing the number of
  // writes to the DIPS db.
  bool ShouldUpdateTimestamp(base::optional_ref<const base::Time> last_time,
                             base::Time now);

  raw_ptr<const base::TickClock> tick_clock_;
  raw_ptr<const base::Clock> clock_;
  raw_ptr<BtmBounceDetectorDelegate> delegate_;
  std::optional<ClientBounceDetectionState> client_detection_state_;
  BtmRedirectContext committed_redirect_context_;
  base::RetainingOneShotTimer client_bounce_detection_timer_;
};

// Holds a pointer to a redirect chain, and uses a timer to delay BTM's
// processing of the chain, to give some time for "late" cookie accesses to be
// reported first.
class DelayedChainHandler {
 public:
  explicit DelayedChainHandler(BtmRedirectChainHandler handler);
  ~DelayedChainHandler();

  void HandleRedirectChain(std::vector<BtmRedirectInfoPtr> redirects,
                           BtmRedirectChainInfoPtr chain);
  [[nodiscard]] bool AddLateCookieAccess(const GURL& url, CookieOperation op);
  void HandlePreviousChainNow() {
    HandlePreviousChainNowImpl(/*timer_fired=*/false);
  }

 private:
  void HandlePreviousChainNowImpl(bool timer_fired);

  BtmRedirectChainHandler handler_;
  std::optional<
      std::pair<std::vector<BtmRedirectInfoPtr>, BtmRedirectChainInfoPtr>>
      prev_chain_pair_;
  base::RetainingOneShotTimer timer_;
};

// Attached to a `WebContents` and observes its navigations to detect chains of
// server and client redirects. Notifies `RedirectChainDetector::Observer`s
// when a redirect chain has completed.
//
// TODO: crbug.com/324573485 - move to separate file.
class CONTENT_EXPORT RedirectChainDetector
    : public WebContentsObserver,
      public WebContentsUserData<RedirectChainDetector>,
      public BtmBounceDetectorDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a navigation has committed and the redirect context has been
    // updated. (If you override WebContentsObserver::DidFinishNavigation()
    // directly, you could be called before the context has been updated.)
    virtual void OnNavigationCommitted(NavigationHandle* navigation_handle) {}
    // Called when any redirect chain ends, including ones that end with an
    // uncommitted navigation.
    virtual void OnRedirectChainEnded(const std::vector<BtmRedirectInfoPtr>&,
                                      const BtmRedirectChainInfo&) {}
    // Called before OnRedirectChainEnded() with set of redirector sites in the
    // chain, omitting the initial and final sites.
    // TODO(rtarpine) - replace with more general purpose method
    virtual void ReportRedirectors(const std::set<std::string>& sites) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

  ~RedirectChainDetector() override;

  void SetRedirectChainHandlerForTesting(BtmRedirectChainHandler handler) {
    detector_.SetRedirectChainHandlerForTesting(handler);
  }

  // Returns state for the in-progress redirect chain.
  const BtmRedirectContext& CommittedRedirectContext() const {
    return detector_.CommittedRedirectContext();
  }

  void SetClockForTesting(base::Clock* clock) {
    detector_.SetClockForTesting(clock);
  }

 private:
  explicit RedirectChainDetector(WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class WebContentsUserData<RedirectChainDetector>;

  // BtmBounceDetectorDelegate overrides:
  GURL GetLastCommittedURL() const override;
  ukm::SourceId GetLastCommittedSourceId() const override;
  void HandleRedirectChain(std::vector<BtmRedirectInfoPtr> redirects,
                           BtmRedirectChainInfoPtr chain) override;
  void ReportRedirectors(std::set<std::string> sites) override;
  bool Are3PcsGenerallyEnabled() const override;
  // End BtmBounceDetectorDelegate overrides.

  // Start WebContentsObserver overrides:
  void PrimaryPageChanged(Page& page) override;
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  // See the `WebContentsObserver` declarations for `OnCookiesAccessed` for when
  // this overload is called vs. the other.
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;
  // See the `WebContentsObserver` declarations for `OnCookiesAccessed` for when
  // this overload is called vs. the other.
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override;
  void NotifyStorageAccessed(RenderFrameHost* render_frame_host,
                             blink::mojom::StorageTypeAccessed storage_type,
                             bool blocked) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void FrameReceivedUserActivation(RenderFrameHost* render_frame_host) override;
  void WebAuthnAssertionRequestSucceeded(
      RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  // End WebContentsObserver overrides:

  void NotifyOnRedirectChainEnded(std::vector<BtmRedirectInfoPtr> redirects,
                                  BtmRedirectChainInfoPtr chain);

  BtmBounceDetector detector_;
  DelayedChainHandler delayed_handler_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<RedirectChainDetector> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Populates the BTM database with site metadata, which is then used by the
// `BtmService` to identify sites that perform bounce tracking and delete their
// storage.
class CONTENT_EXPORT BtmWebContentsObserver
    : public WebContentsObserver,
      public WebContentsUserData<BtmWebContentsObserver>,
      public RedirectChainDetector::Observer {
 public:
  static void MaybeCreateForWebContents(WebContents* web_contents);

  ~BtmWebContentsObserver() override;

  // Use the passed handler instead of BtmWebContentsObserver::EmitBtmIssue().
  void SetIssueReportingCallbackForTesting(BtmIssueReportingCallback callback) {
    issue_reporting_callback_ = callback;
  }

  // TODO(rtarpine): make this take a Clock&.
  void SetClockForTesting(base::Clock* clock) {
    DCHECK(btm_service_);
    btm_service_->storage()
        ->AsyncCall(&BtmStorage::SetClockForTesting)
        .WithArgs(clock);
    RedirectChainDetector::FromWebContents(web_contents())
        ->SetClockForTesting(clock);
    clock_ = *clock;
  }

 private:
  BtmWebContentsObserver(WebContents* web_contents,
                         BtmServiceImpl* dips_service);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class WebContentsUserData<BtmWebContentsObserver>;

  // Emits a BTM issue to the inspector (e.g., DevTools).
  void EmitBtmIssue(const std::set<std::string>& sites);

  void RecordEvent(BtmRecordedEvent event,
                   const GURL& url,
                   const base::Time& time);
  void OnStatefulBounce(const GURL& final_url);

  // Start RedirectChainDetector::Observer overrides:
  void ReportRedirectors(const std::set<std::string>& sites) override;
  void OnRedirectChainEnded(const std::vector<BtmRedirectInfoPtr>& redirects,
                            const BtmRedirectChainInfo& chain) override;
  // End RedirectChainDetector::Observer overrides.

  // Start WebContentsObserver overrides:
  void PrimaryPageChanged(Page& page) override;
  void FrameReceivedUserActivation(RenderFrameHost* render_frame_host) override;
  void WebAuthnAssertionRequestSucceeded(
      RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  // End WebContentsObserver overrides:

  raw_ptr<RedirectChainDetector> detector_;
  // raw_ptr<> is safe here because BtmServiceImpl is a KeyedService,
  // associated with the BrowserContext/Profile which will outlive the
  // WebContents that BtmWebContentsObserver is observing.
  raw_ptr<BtmServiceImpl> btm_service_;
  raw_ref<base::Clock> clock_{*base::DefaultClock::GetInstance()};
  BtmIssueReportingCallback issue_reporting_callback_;

  std::optional<std::string> last_committed_site_;
  std::optional<base::Time> last_storage_timestamp_;
  std::optional<base::Time> last_interaction_timestamp_;

  base::WeakPtrFactory<BtmWebContentsObserver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

namespace btm {

ukm::SourceId GetRedirectSourceId(NavigationHandle* navigation_handle,
                                  size_t index);

CONTENT_EXPORT bool IsOrWasInPrimaryPage(RenderFrameHost& render_frame_host);

// Sets the `has_3pc_exception` field of each element of `redirects`.
CONTENT_EXPORT void Populate3PcExceptions(
    BrowserContext* browser_context,
    WebContents* web_contents,
    const GURL& initial_url,
    const GURL& final_url,
    base::span<BtmRedirectInfoPtr> redirects);

// Checks whether third-party cookies are generally enabled within this browser
// session.
//
// Any bounce tracking mitigations processing should be short-circuited when
// TPCs are enabled by default.
CONTENT_EXPORT bool Are3PcsGenerallyEnabled(BrowserContext* browser_context,
                                            WebContents* web_contents);
}  // namespace btm

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_BOUNCE_DETECTOR_H_
