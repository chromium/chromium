// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_

#include <list>
#include <map>
#include <optional>

#include "base/containers/unique_ptr_adapters.h"
#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

class SafeBrowsingClient;
@protocol SafeBrowsingTabHelperDelegate;

// Used to identify which redirect chain logic branch should be used. For
// example, `kPendingMainFrame` will use logic related to
// `pending_main_frame_redirect_chain_`.
enum class RedirectChain {
  kPendingMainFrame = 0,
  kToBeCommitted = 1,
  kCommitted = 2,
};

// Filters used to look for specific types of queries while iterating through a
// redirect chain. These filters can be used to affect if a partially completed
// policy decision is made. For example, `kSyncQueries` can be used to see if
// all sync queries were completed and return an overall policy decision for
// sync checks.
enum class RedirectChainFilter {
  kAllQueries = 0,
  kSyncQueries = 1,
};

// A tab helper that uses Safe Browsing to check whether URLs that are being
// navigated to are unsafe.
class SafeBrowsingTabHelper
    : public web::WebStateUserData<SafeBrowsingTabHelper> {
 public:
  ~SafeBrowsingTabHelper() override;

  SafeBrowsingTabHelper(const SafeBrowsingTabHelper&) = delete;
  SafeBrowsingTabHelper& operator=(const SafeBrowsingTabHelper&) = delete;

  // Sets delegate for safe browsing tab helper.
  void SetDelegate(id<SafeBrowsingTabHelperDelegate> delegate);
  // Removes delegate. Sets delegate to nil.
  void RemoveDelegate();
  // Tells delegate to open safe browsing settings.
  void OpenSafeBrowsingSettings();
  // Tells delegate to show enhanced safe browsing promo.
  void ShowEnhancedSafeBrowsingInfobar();

 private:
  friend class web::WebStateUserData<SafeBrowsingTabHelper>;

  SafeBrowsingTabHelper(web::WebState* web_state, SafeBrowsingClient* client);

  // A WebStatePolicyDecider that queries the SafeBrowsing database on each
  // request, always allows the request, but uses the result of the
  // SafeBrowsing check to determine whether to allow the corresponding
  // response.
  class PolicyDecider : public web::WebStatePolicyDecider {
   public:
    PolicyDecider(web::WebState* web_state, SafeBrowsingClient* client);
    ~PolicyDecider() override;

    // Returns whether `query` is still relevant.  May return false if
    // navigations occurred before the URL check has finished.
    bool IsQueryStale(const SafeBrowsingQueryManager::Query& query);

    // Decides if a page is reloaded on commit. Used when an async check is
    // completed while an unsafe query is in the
    // `to_be_committed_redirect_chain_`.
    bool ShouldReloadOnCommit();

    // Returns whether a query contained in `query_data` is still relevant. May
    // return false if navigations occurred before the URL check has finished.
    bool IsQueryStale(const SafeBrowsingQueryManager::QueryData& query_data);

    // Clears and moves `to_be_committed_redirect_chain_` to
    // `committed_redirect_chain_`.
    void SetCommittedRedirectChain();

    // Reloads the page. Used when a reload is necessary for triggering an error
    // page.
    void ReloadPage();

    // Returns a policy decision based on query `result`.
    web::WebStatePolicyDecider::PolicyDecision CreatePolicyDecision(
        const SafeBrowsingQueryManager::Query& query,
        const SafeBrowsingQueryManager::Result& result,
        web::WebState* web_state);

    // Stores `policy_decision` for `query`.  `query` must not be stale.
    // `performed_check` is the type of check that was performed when deciding
    // the query.
    void HandlePolicyDecision(
        const SafeBrowsingQueryManager::Query& query,
        const web::WebStatePolicyDecider::PolicyDecision& policy_decision,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check);

    // Uses `query_data` to store the `policy_decision` for a non-stale query
    // taking into account if the check is a sync or async check.
    void HandlePolicyDecision(
        const SafeBrowsingQueryManager::QueryData& query_data,
        const web::WebStatePolicyDecider::PolicyDecision& policy_decision);

    // Notifies the policy decider that a new main frame document has been
    // loaded.
    void UpdateForMainFrameDocumentChange();

    // Notifies the policy decider that the most recent main frame query is
    // a server redirect of the previous main frame query.
    void UpdateForMainFrameServerRedirect();

   private:
    // Represents a single Safe Browsing query URL, along with the corresponding
    // decision once it's received, the callback to invoke once the decision
    // is known, and tracks if the async or sync check for the respective query
    // is complete.
    struct MainFrameUrlQuery {
      explicit MainFrameUrlQuery(const GURL& url);
      MainFrameUrlQuery(MainFrameUrlQuery&& query);
      MainFrameUrlQuery& operator=(MainFrameUrlQuery&& other);
      ~MainFrameUrlQuery();

      GURL url;
      std::optional<web::WebStatePolicyDecider::PolicyDecision> decision;
      web::WebStatePolicyDecider::PolicyDecisionCallback response_callback;
      bool sync_check_complete = false;
      bool async_check_complete = false;

      // The time at which a navigation was delayed waiting for the result of
      // this query.
      base::TimeTicks delay_start_time;
    };

    // web::WebStatePolicyDecider implementation
    void ShouldAllowRequest(
        NSURLRequest* request,
        web::WebStatePolicyDecider::RequestInfo request_info,
        web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;
    void ShouldAllowResponse(
        NSURLResponse* response,
        web::WebStatePolicyDecider::ResponseInfo response_info,
        web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

    // Returns the oldest query for `url` that has not yet received a decision.
    // If there are no queries for `url` or if all such queries have already
    // been decided, returns null.
    MainFrameUrlQuery* GetOldestPendingMainFrameQuery(const GURL& url);

    // Returns the oldest pending main frame query for `query_data` that has not
    // yet received a decision taking into account `query_data` to distinguish
    // sync and async queries. If there are no queries for `query_data` or if
    // all relevant queries have already been decided, returns null.
    MainFrameUrlQuery* GetOldestPendingMainFrameQuery(
        const SafeBrowsingQueryManager::QueryData& query_data);

    // Returns the oldest pending query for the
    // `to_be_committed_redirect_chain_` that has not yet received a decision
    // taking into account `query_data` to distinguish sync and async queries.
    // If there are no queries for `query_data` or if all relevant queries have
    // already been decided, returns null.
    MainFrameUrlQuery* GetOldestPendingToBeCommittedQuery(
        const SafeBrowsingQueryManager::QueryData& query_data);

    // Returns the oldest pending query for the `committed_redirect_chain_` that
    // has not yet received a decision taking into account `query_data` to
    // distinguish sync and async queries. If there are no queries for
    // `query_data` or if all relevant queries have already been decided,
    // returns null.
    MainFrameUrlQuery* GetOldestPendingCommittedQuery(
        const SafeBrowsingQueryManager::QueryData& query_data);

    // Iterates through the `redirect_chain` and uses `query_data` to return an
    // unanswered sync or async query.
    MainFrameUrlQuery* GetUnansweredQueryForRedirectChain(
        RedirectChain redirect_chain,
        const SafeBrowsingQueryManager::QueryData& query_data);

    // Callback invoked when a main frame query for `url` has finished with
    // `decision` after performing a check of type `performed_check`.
    void OnMainFrameUrlQueryDecided(
        const GURL& url,
        web::WebStatePolicyDecider::PolicyDecision decision,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check);

    // Callback invoked when a main frame query using `query_data` has finished
    // with `decision` after performing a sync check.
    void OnMainFrameUrlSyncQueryDecided(
        const SafeBrowsingQueryManager::QueryData& query_data,
        web::WebStatePolicyDecider::PolicyDecision decision);

    // Decisions made from async checks aren't required to allow a navigation to
    // proceed. Therefore, this function doesn't necessarily run the response
    // callback provided from `PolicyDecider::ShouldAllowResponse()`. The main
    // purpose of `OnMainFrameUrlAsyncQueryDecided()` is to update the policy
    // `decision` and to potentially block a navigation if an unsafe decision is
    // received from an async check.
    void OnMainFrameUrlAsyncQueryDecided(
        const SafeBrowsingQueryManager::QueryData& query_data,
        web::WebStatePolicyDecider::PolicyDecision decision);

    // Updates a MainFrameUrlQuery's components and the related policy
    // `decision`.
    void UpdateQuery(MainFrameUrlQuery* query,
                     QueryType query_type,
                     web::WebStatePolicyDecider::PolicyDecision decision);

    // Returns the policy decision determined by the results of queries for URLs
    // in the main-frame redirect chain and the `pending_main_frame_query`. If
    // at least one such query has received a decision to cancel the navigation,
    // the overall decision is to cancel, even if some queries have not yet
    // received a response. If all queries have received a decision to allow the
    // navigation, the overall decision is to allow the navigation. Otherwise,
    // the overall decision depends on query results that have not yet been
    // received, so std::nullopt is returned.
    std::optional<web::WebStatePolicyDecider::PolicyDecision>
    MainFrameRedirectChainDecision();

    // Returns the policy decision determined by the results of queries for URLs
    // in a `redirect_chain`, and a redirect chain filter. Regardless of the
    // `filter`, if at least one such query has received a decision to cancel
    // the navigation, the overall decision is to cancel, even if some queries
    // have not yet received a response. After applying the `filter`, if the
    // relevant queries have a decision to allow the navigation, then the
    // decision is to allow the navigation. Otherwise, the overall decision
    // depends on query results that have not yet been received, so std::nullopt
    // is returned.
    std::optional<web::WebStatePolicyDecider::PolicyDecision>
    RedirectChainDecisionWithFilter(RedirectChain redirect_chain,
                                    RedirectChainFilter filter);

    // The sync_check_complete and async_check_complete from `query` are used to
    // detect if a query belongs to a certain RedirectChainFilter. Returns
    // std::nullopt if `query` is not apart of the `filter`. If the query
    // belongs, `decision` is returned.
    std::optional<web::WebStatePolicyDecider::PolicyDecision>
    QueryDecisionFromFilter(
        const MainFrameUrlQuery& query,
        std::optional<web::WebStatePolicyDecider::PolicyDecision> decision,
        RedirectChainFilter filter);

    // Moves `pending_main_frame_redirect_chain_` to
    // `to_be_committed_redirect_chain_`.
    void UpdateToBeCommittedRedirectChain();

    // The URL check query manager.
    raw_ptr<SafeBrowsingQueryManager> query_manager_;
    // The safe browsing client.
    raw_ptr<SafeBrowsingClient> client_ = nullptr;
    // The pending query for the main frame navigation, if any.
    std::optional<MainFrameUrlQuery> pending_main_frame_query_;
    // The previous query for main frame, navigation, if any. This is tracked
    // as a potential redirect source for the current
    // `pending_main_frame_query_`.
    std::optional<MainFrameUrlQuery> previous_main_frame_query_;
    // A list of queries corresponding to the redirect chain leading to the
    // current `pending_main_frame_query_`. This does not include
    // `pending_main_frame_query_` itself.
    std::list<MainFrameUrlQuery> pending_main_frame_redirect_chain_;
    // A list of queries corresponding to the redirect chain saved at
    // `ShouldAllowResponse()` and before `DidFinishNavigation()`.
    std::list<MainFrameUrlQuery> to_be_committed_redirect_chain_;
    // A list of queries corresponding to the redirect chain saved after
    // DidFinishNavigation() is called.
    std::list<MainFrameUrlQuery> committed_redirect_chain_;
    bool reload_page_on_commit_ = false;
  };

  // Helper object that observes results of URL check queries.
  class QueryObserver : public SafeBrowsingQueryManager::Observer {
   public:
    QueryObserver(web::WebState* web_state, PolicyDecider* decider);
    ~QueryObserver() override;

   private:
    // SafeBrowsingQueryManager::Observer:
    void SafeBrowsingQueryFinished(
        SafeBrowsingQueryManager* manager,
        const SafeBrowsingQueryManager::Query& query,
        const SafeBrowsingQueryManager::Result& result,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check) override;
    void SafeBrowsingSyncQueryFinished(
        const SafeBrowsingQueryManager::QueryData& query_data) override;
    void SafeBrowsingAsyncQueryFinished(
        const SafeBrowsingQueryManager::QueryData& query_data) override;
    void SafeBrowsingQueryManagerDestroyed(
        SafeBrowsingQueryManager* manager) override;

    raw_ptr<web::WebState> web_state_ = nullptr;
    raw_ptr<PolicyDecider> policy_decider_ = nullptr;
    base::ScopedObservation<SafeBrowsingQueryManager,
                            SafeBrowsingQueryManager::Observer>
        scoped_observation_{this};
  };

  // Helper object that resets state for the policy decider when a navigation is
  // finished, and notifies the policy decider about navigation redirects so
  // that the decider can associate queries that are part of a redirection
  // chain.
  class NavigationObserver : public web::WebStateObserver {
   public:
    NavigationObserver(web::WebState* web_state, PolicyDecider* policy_decider);
    ~NavigationObserver() override;

   private:
    // web::WebStateObserver:
    void DidRedirectNavigation(
        web::WebState* web_state,
        web::NavigationContext* navigation_context) override;
    void DidFinishNavigation(
        web::WebState* web_state,
        web::NavigationContext* navigation_context) override;
    void WebStateDestroyed(web::WebState* web_state) override;

    raw_ptr<PolicyDecider> policy_decider_ = nullptr;
    base::ScopedObservation<web::WebState, web::WebStateObserver>
        scoped_observation_{this};
  };

  PolicyDecider policy_decider_;
  QueryObserver query_observer_;
  NavigationObserver navigation_observer_;
  __weak id<SafeBrowsingTabHelperDelegate> delegate_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
