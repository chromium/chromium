// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_QUERY_MANAGER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_QUERY_MANAGER_H_

#include <map>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace web {
class NavigationItem;
}

class SafeBrowsingClient;

// A helper object that manages the Safe Browsing URL queries for a single
// WebState.
class SafeBrowsingQueryManager
    : public web::WebStateUserData<SafeBrowsingQueryManager> {
 public:
  // Struct used to trigger URL check queries.
  struct Query {
    explicit Query(const GURL& url,
                   const std::string& http_method,
                   int main_frame_item_id = -1);
    Query() = delete;
    Query(const Query&);
    virtual ~Query();

    // Operator overloaded so it can be compared with std::less.
    bool operator<(const Query& other) const;

    // Whether the query is for the main frame.
    bool IsMainFrame() const;

    // The URL whose safety is being checked.
    const GURL url;
    // The HTTP method.
    const std::string http_method;
    // The ID of the NavigationItem triggering the URL check.  -1 for main-frame
    // URL checks.
    const int main_frame_item_id;
    // The unique ID for the query.
    const size_t query_id;
  };

  // Struct used to store URL check results.
  struct Result {
    Result();
    Result(const Result&);
    Result& operator=(const Result&);
    ~Result();

    // Whether navigations to the URL should proceed.
    bool proceed = false;
    // Whether an error page should be shown for the URL.
    bool show_error_page = false;
    // The UnsafeResource created for the URL check, if any.
    std::optional<security_interstitials::UnsafeResource> resource;
  };

  // Observer class for the query manager.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies observers that `query` has completed with `result` after
    // performing a check of type `performed_check`.
    virtual void SafeBrowsingQueryFinished(
        SafeBrowsingQueryManager* manager,
        const Query& query,
        const Result& result,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check) {}

    // Called when `manager` is about to be destroyed.
    virtual void SafeBrowsingQueryManagerDestroyed(
        SafeBrowsingQueryManager* manager) {}
  };

  ~SafeBrowsingQueryManager() override;

  SafeBrowsingQueryManager(const SafeBrowsingQueryManager&) = delete;
  SafeBrowsingQueryManager& operator=(const SafeBrowsingQueryManager&) = delete;

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts the URL check for `query`.
  void StartQuery(const Query& query);

  // Stores `resource` in the result for the query that triggered the URL check.
  // No-ops if there is no active query for `resource`'s URL.
  void StoreUnsafeResource(
      const security_interstitials::UnsafeResource& resource);

 private:
  friend class web::WebStateUserData<SafeBrowsingQueryManager>;

  SafeBrowsingQueryManager(web::WebState* web_state,
                           SafeBrowsingClient* client);

  // Queries the Safe Browsing database using SafeBrowsingUrlCheckerImpls. This
  // class may be constructed on the UI thread but otherwise must only be used
  // and destroyed on the IO thread. If kSafeBrowsingOnUIThread is enabled this
  // is used and destroyed on the UI thread.
  class UrlCheckerClient : public base::SupportsWeakPtr<UrlCheckerClient> {
   public:
    UrlCheckerClient();
    ~UrlCheckerClient();

    UrlCheckerClient(const UrlCheckerClient&) = delete;
    UrlCheckerClient& operator=(const UrlCheckerClient&) = delete;

    // Queries the database using the given `url_checker`, for a request with
    // the given `url` and the given HTTP `method`. After receiving a result
    // from the database, runs the given `callback` on the UI thread with the
    // result.
    void CheckUrl(
        std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker,
        const GURL& url,
        const std::string& method,
        base::OnceCallback<void(bool proceed,
                                bool show_error_page,
                                safe_browsing::SafeBrowsingUrlCheckerImpl::
                                    PerformedCheck performed_check)> callback);

   private:
    // Called by `url_checker` with the initial result of performing a url
    // check. `url_checker` must be non-null. This is an implementation of
    // SafeBrowsingUrlCheckerImpl::NativeUrlCheckCallBack. `slow_check_notifier`
    // is an out-parameter; when a non-null value is passed in, it is set to a
    // callback that receives the final result of the url check.
    void OnCheckUrlResult(
        safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
        safe_browsing::SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier*
            slow_check_notifier,
        bool proceed,
        bool showed_interstitial,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check);

    // Called by `url_checker` with the final result of performing a url check.
    // `url_checker` must be non-null. This is an implementation of
    // SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier.
    void OnCheckComplete(
        safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
        bool proceed,
        bool showed_interstitial,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check);

    // This maps SafeBrowsingUrlCheckerImpls that have started but not completed
    // a url check to the callback that should be invoked once the url check is
    // complete.
    base::flat_map<std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>,
                   base::OnceCallback<void(
                       bool proceed,
                       bool show_error_page,
                       safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
                           performed_check)>,
                   base::UniquePtrComparator>
        active_url_checkers_;
  };

  // Used as the completion callback for URL queries executed by
  // `url_checker_client_`.
  void UrlCheckFinished(
      const Query query,
      bool proceed,
      bool show_error_page,
      safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
          performed_check);

  // The WebState whose URL queries are being managed.
  web::WebState* web_state_ = nullptr;
  // The safe browsing client.
  SafeBrowsingClient* client_ = nullptr;
  // The checker client.  Used to communicate with the database on the IO
  // thread. If kSafeBrowsingOnUIThread is enabled it'll be used on the UI
  // thread.
  std::unique_ptr<UrlCheckerClient> url_checker_client_;
  // The results for each active query.
  std::map<const Query, Result> results_;
  // The observers.
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
  // The weak pointer factory.
  base::WeakPtrFactory<SafeBrowsingQueryManager> weak_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_QUERY_MANAGER_H_
