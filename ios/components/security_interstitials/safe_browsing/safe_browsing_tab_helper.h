// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_TAB_HELPER_H_

#include <list>
#include <map>
#include <optional>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace web {
class NavigationItem;
}

class SafeBrowsingClient;
@protocol SafeBrowsingTabHelperDelegate;

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

 private:
  friend class web::WebStateUserData<SafeBrowsingTabHelper>;

  SafeBrowsingTabHelper(web::WebState* web_state, SafeBrowsingClient* client);

  // A WebStatePolicyDecider that queries the SafeBrowsing database on each
  // request, always allows the request, but uses the result of the
  // SafeBrowsing check to determine whether to allow the corresponding
  // response.
  class PolicyDecider : public web::WebStatePolicyDecider,
                        public base::SupportsWeakPtr<PolicyDecider> {
   public:
    PolicyDecider(web::WebState* web_state, SafeBrowsingClient* client);
    ~PolicyDecider() override;

    // Returns whether `query` is still relevant.  May return false if
    // navigations occurred before the URL check has finished.
    bool IsQueryStale(const SafeBrowsingQueryManager::Query& query);

    // Stores `policy_decision` for `query`.  `query` must not be stale.
    // `performed_check` is the type of check that was performed when deciding
    // the query.
    void HandlePolicyDecision(
        const SafeBrowsingQueryManager::Query& query,
        const web::WebStatePolicyDecider::PolicyDecision& policy_decision,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check);

    // Notifies the policy decider that a new main frame document has been
    // loaded.
    void UpdateForMainFrameDocumentChange();

    // Notifies the policy decider that the most recent main frame query is
    // a server redirect of the previous main frame query.
    void UpdateForMainFrameServerRedirect();

   private:
    // Represents a single Safe Browsing query URL, along with the corresponding
    // decision once it's received, and the callback to invoke once the decision
    // is known.
    struct MainFrameUrlQuery {
      explicit MainFrameUrlQuery(const GURL& url);
      MainFrameUrlQuery(MainFrameUrlQuery&& query);
      MainFrameUrlQuery& operator=(MainFrameUrlQuery&& other);
      ~MainFrameUrlQuery();

      GURL url;
      std::optional<web::WebStatePolicyDecider::PolicyDecision> decision;
      web::WebStatePolicyDecider::PolicyDecisionCallback response_callback;

      // The time at which a navigation was delayed waiting for the result of
      // this query.
      base::TimeTicks delay_start_time;
    };

    // Represents the policy decision for a URL loaded in a sub frame.
    // ShouldAllowRequest() is not executed for consecutive loads of the same
    // URL, so it's possible for multiple sub frames to load the same URL and
    // share the policy decision generated from a single ShouldAllowRequest()
    // call.  If multiple ShouldAllowResponse() calls are received before the
    // url check has finished, they are added to `response_callbacks`.
    struct SubFrameUrlQuery {
      SubFrameUrlQuery();
      SubFrameUrlQuery(SubFrameUrlQuery&& decision);
      ~SubFrameUrlQuery();

      std::optional<web::WebStatePolicyDecider::PolicyDecision> decision;
      std::list<web::WebStatePolicyDecider::PolicyDecisionCallback>
          response_callbacks;

      // The times at which navigations were delayed waiting for the result of
      // this query. This list has the same ordering as `response_callbacks`.
      std::list<base::TimeTicks> delay_start_times;
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

    // Implementations of ShouldAllowResponse() for main frame and sub frame
    // navigations.
    void HandleMainFrameResponsePolicy(
        const GURL& url,
        web::WebStatePolicyDecider::PolicyDecisionCallback callback);
    void HandleSubFrameResponsePolicy(
        const GURL& url,
        web::WebStatePolicyDecider::PolicyDecisionCallback callback);

    // Returns the oldest query for `url` that has not yet received a decision.
    // If there are no queries for `url` or if all such queries have already
    // been decided, returns null.
    MainFrameUrlQuery* GetOldestPendingMainFrameQuery(const GURL& url);

    // Callback invoked when a main frame query for `url` has finished with
    // `decision` after performing a check of type `performed_check`.
    void OnMainFrameUrlQueryDecided(
        const GURL& url,
        web::WebStatePolicyDecider::PolicyDecision decision,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check);

    // Callback invoked when a sub frame url query for the NavigationItem with
    // `navigation_item_id` has finished with `decision` after performing a
    // check of type `performed_check`.
    void OnSubFrameUrlQueryDecided(
        const GURL& url,
        web::WebStatePolicyDecider::PolicyDecision decision,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check);

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

    // The URL check query manager.
    SafeBrowsingQueryManager* query_manager_;
    // The safe browsing client.
    SafeBrowsingClient* client_ = nullptr;
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
    // A map associating the pending policy decisions for each URL loaded into a
    // sub frame.
    std::map<const GURL, SubFrameUrlQuery> pending_sub_frame_queries_;
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
    void SafeBrowsingQueryManagerDestroyed(
        SafeBrowsingQueryManager* manager) override;

    web::WebState* web_state_ = nullptr;
    PolicyDecider* policy_decider_ = nullptr;
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

    PolicyDecider* policy_decider_ = nullptr;
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
