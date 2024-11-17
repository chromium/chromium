// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"

#import "base/check_op.h"
#import "base/functional/callback_helpers.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/mojom/fetch_api.mojom.h"

using security_interstitials::UnsafeResource;

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingQueryManager)

namespace {
// Creates a unique ID for a new Query.
size_t CreateQueryID() {
  static size_t query_id = 0;
  return query_id++;
}
}  // namespace

#pragma mark - SafeBrowsingQueryManager

SafeBrowsingQueryManager::SafeBrowsingQueryManager(web::WebState* web_state,
                                                   SafeBrowsingClient* client)
    : web_state_(web_state),
      client_(client),
      url_checker_client_(std::make_unique<UrlCheckerClient>()) {
  DCHECK(web_state_);
}

SafeBrowsingQueryManager::~SafeBrowsingQueryManager() {
  for (auto& observer : observers_) {
    observer.SafeBrowsingQueryManagerDestroyed(this);
  }
}

void SafeBrowsingQueryManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SafeBrowsingQueryManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SafeBrowsingQueryManager::StartQuery(const Query& query) {
  // Store the query request.
  results_.insert({query, Result()});

  // Create a URL checker and perform the query on the IO thread.
  network::mojom::RequestDestination request_destination =
      network::mojom::RequestDestination::kDocument;
  SafeBrowsingService* safe_browsing_service =
      client_->GetSafeBrowsingService();

  bool async_check_enabled =
      safe_browsing_service->ShouldCreateAsyncChecker(web_state_, client_);

  if (async_check_enabled) {
    base::OnceCallback<void(
        bool proceed, bool show_error_page,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check)>
        sync_callback =
            base::BindOnce(&SafeBrowsingQueryManager::UrlCheckFinished,
                           weak_factory_.GetWeakPtr(), query, QueryType::kSync);
    base::OnceCallback<void(
        bool proceed, bool show_error_page,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check)>
        async_callback = base::BindOnce(
            &SafeBrowsingQueryManager::UrlCheckFinished,
            weak_factory_.GetWeakPtr(), query, QueryType::kAsync);

    std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> sync_checker =
        safe_browsing_service->CreateSyncChecker(request_destination,
                                                 web_state_, client_);

    std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> async_checker =
        safe_browsing_service->CreateAsyncChecker(request_destination,
                                                  web_state_, client_);

    url_checker_client_->CheckUrl(std::move(sync_checker), query.url,
                                  query.http_method, std::move(sync_callback));
    url_checker_client_->CheckUrl(std::move(async_checker), query.url,
                                  query.http_method, std::move(async_callback));
  } else {
    base::OnceCallback<void(
        bool proceed, bool show_error_page,
        safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
            performed_check)>
        callback =
            base::BindOnce(&SafeBrowsingQueryManager::UrlCheckFinished,
                           weak_factory_.GetWeakPtr(), query, QueryType::kSync);

    std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker =
        safe_browsing_service->CreateUrlChecker(request_destination, web_state_,
                                                client_);

    url_checker_client_->CheckUrl(std::move(url_checker), query.url,
                                  query.http_method, std::move(callback));
  }
}

void SafeBrowsingQueryManager::StoreUnsafeResource(
    const UnsafeResource& resource) {
  // Responses to repeated queries can arrive in arbitrary order, not
  // necessarily in the same order as the queries are made. This means
  // that when there are repeated pending queries (e.g., when a page has
  // multiple iframes with the same URL), it is not possible to determine
  // which of these queries will receive a response first. As a result,
  // `resource` must be stored with every corresponding query, not just the
  // first.
  for (auto& pair : results_) {
    if (pair.first.url == resource.url && !pair.second.resource) {
      pair.second.resource = resource;
    }
  }
}

#pragma mark Private

void SafeBrowsingQueryManager::UrlCheckFinished(
    const Query query,
    const QueryType type,
    bool proceed,
    bool show_error_page,
    safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  auto query_result_pair = results_.find(query);
  DCHECK(query_result_pair != results_.end());

  // Store the query result.
  Result& result = query_result_pair->second;
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    if (result.sync_check_complete || result.async_check_complete) {
      // If one result has already been received, combine results.
      result.proceed = result.proceed && proceed;
      result.show_error_page = result.show_error_page || show_error_page;
    } else {
      result.proceed = proceed;
      result.show_error_page = show_error_page;
    }
  } else {
    result.proceed = proceed;
    result.show_error_page = show_error_page;
  }
  // If an error page is requested, an UnsafeResource must be stored before
  // the execution of its completion block.
  DCHECK(!show_error_page || result.resource);

  // Notify observers of the completed URL check. `this` might get destroyed
  // when an observer is notified.
  auto weak_this = weak_factory_.GetWeakPtr();

  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    if (type == QueryType::kAsync) {
      result.async_check_complete = true;
    } else if (type == QueryType::kSync) {
      result.sync_check_complete = true;
    }

    for (auto& observer : observers_) {
      const QueryData& query_data =
          QueryData(this, query, type, result, performed_check);
      if (type == QueryType::kAsync) {
        observer.SafeBrowsingAsyncQueryFinished(query_data);
      } else if (type == QueryType::kSync) {
        observer.SafeBrowsingSyncQueryFinished(query_data);
      }

      if (!weak_this) {
        return;
      }
    }

    // Clear out the state since the query is finished.
    if (result.sync_check_complete && result.async_check_complete) {
      results_.erase(query_result_pair);
    }
  } else {
    for (auto& observer : observers_) {
      observer.SafeBrowsingQueryFinished(this, query, result, performed_check);
      if (!weak_this) {
        return;
      }
    }

    // Clear out the state since the query is finished.
    results_.erase(query_result_pair);
  }
}

#pragma mark - SafeBrowsingQueryManager::Query

SafeBrowsingQueryManager::Query::Query(const GURL& url,
                                       const std::string& http_method)
    : url(url), http_method(http_method), query_id(CreateQueryID()) {}

SafeBrowsingQueryManager::Query::Query(const Query&) = default;

SafeBrowsingQueryManager::Query::~Query() = default;

bool SafeBrowsingQueryManager::Query::operator<(const Query& other) const {
  return query_id < other.query_id;
}

#pragma mark - SafeBrowsingQueryManager::Result

SafeBrowsingQueryManager::Result::Result() = default;

SafeBrowsingQueryManager::Result::Result(const Result&) = default;

SafeBrowsingQueryManager::Result& SafeBrowsingQueryManager::Result::operator=(
    const Result&) = default;

SafeBrowsingQueryManager::Result::~Result() = default;

#pragma mark - SafeBrowsingQueryManager::QueryData

SafeBrowsingQueryManager::QueryData::QueryData(
    SafeBrowsingQueryManager* manager,
    const SafeBrowsingQueryManager::Query& query,
    const QueryType type,
    const SafeBrowsingQueryManager::Result& result,
    safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check)
    : manager(manager),
      query(query),
      type(type),
      result(result),
      performed_check(performed_check) {}

SafeBrowsingQueryManager::QueryData::QueryData(const QueryData&) = default;

SafeBrowsingQueryManager::QueryData::~QueryData() = default;

#pragma mark - SafeBrowsingQueryManager::UrlCheckerClient

SafeBrowsingQueryManager::UrlCheckerClient::UrlCheckerClient() = default;

SafeBrowsingQueryManager::UrlCheckerClient::~UrlCheckerClient() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
}

void SafeBrowsingQueryManager::UrlCheckerClient::CheckUrl(
    std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker,
    const GURL& url,
    const std::string& method,
    base::OnceCallback<void(bool proceed,
                            bool show_error_page,
                            safe_browsing::SafeBrowsingUrlCheckerImpl::
                                PerformedCheck performed_check)> callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker_ptr =
      url_checker.get();
  active_url_checkers_[std::move(url_checker)] = std::move(callback);
  url_checker_ptr->CheckUrl(url, method,
                            base::BindOnce(&UrlCheckerClient::OnCheckUrlResult,
                                           AsWeakPtr(), url_checker_ptr));
}

void SafeBrowsingQueryManager::UrlCheckerClient::OnCheckUrlResult(
    safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped,
    safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(url_checker);

  auto it = active_url_checkers_.find(url_checker);
  auto callback = std::move(it->second);
  active_url_checkers_.erase(it);
  std::move(callback).Run(proceed, showed_interstitial, performed_check);
  // `callback` may have deleted `this`, the only safe thing is to return.
}
