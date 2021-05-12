// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_query_manager.h"

#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/safe_browsing/safe_browsing_service.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "services/network/public/mojom/fetch_api.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

SafeBrowsingQueryManager::SafeBrowsingQueryManager(web::WebState* web_state)
    : web_state_(web_state),
      url_checker_client_(std::make_unique<UrlCheckerClient>()) {
  DCHECK(web_state_);
}

SafeBrowsingQueryManager::~SafeBrowsingQueryManager() {
  for (auto& observer : observers_) {
    observer.SafeBrowsingQueryManagerDestroyed(this);
  }

  base::DeleteSoon(FROM_HERE, {web::WebThread::IO},
                   url_checker_client_.release());
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
      query.IsMainFrame() ? network::mojom::RequestDestination::kDocument
                          : network::mojom::RequestDestination::kIframe;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker =
      GetApplicationContext()->GetSafeBrowsingService()->CreateUrlChecker(
          request_destination, web_state_);
  base::OnceCallback<void(bool proceed, bool show_error_page)> callback =
      base::BindOnce(&SafeBrowsingQueryManager::UrlCheckFinished,
                     weak_factory_.GetWeakPtr(), query);
  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(&UrlCheckerClient::CheckUrl,
                     url_checker_client_->AsWeakPtr(), std::move(url_checker),
                     query.url, query.http_method, std::move(callback)));
}

void SafeBrowsingQueryManager::StoreUnsafeResource(
    const UnsafeResource& resource) {
  bool is_main_frame = resource.request_destination ==
                       network::mojom::RequestDestination::kDocument;
  auto it = std::find_if(results_.begin(), results_.end(),
                         [&resource, &is_main_frame](const auto& pair) {
                           return pair.first.url == resource.url &&
                                  is_main_frame == pair.first.IsMainFrame() &&
                                  !pair.second.resource;
                         });
  if (it == results_.end())
    return;

  UnsafeResource resource_copy = resource;
  resource_copy.callback = base::DoNothing();
  it->second.resource = resource;
}

#pragma mark Private

void SafeBrowsingQueryManager::UrlCheckFinished(const Query query,
                                                bool proceed,
                                                bool show_error_page) {
  auto query_result_pair = results_.find(query);
  DCHECK(query_result_pair != results_.end());

  // Store the query result.
  Result& result = query_result_pair->second;
  result.proceed = proceed;
  result.show_error_page = show_error_page;

  // If an error page is requested, an UnsafeResource must be stored before the
  // execution of its completion block.
  DCHECK(!show_error_page || result.resource);

  // Notify observers of the completed URL check. |this| might get destroyed
  // when an observer is notified.
  auto weak_this = weak_factory_.GetWeakPtr();
  for (auto& observer : observers_) {
    observer.SafeBrowsingQueryFinished(this, query, result);
    if (!weak_this)
      return;
  }

  // Clear out the state since the query is finished.
  results_.erase(query_result_pair);
}

#pragma mark - SafeBrowsingQueryManager::Query

SafeBrowsingQueryManager::Query::Query(const GURL& url,
                                       const std::string& http_method,
                                       int main_frame_item_id)
    : url(url),
      http_method(http_method),
      main_frame_item_id(main_frame_item_id),
      query_id(CreateQueryID()) {}

SafeBrowsingQueryManager::Query::Query(const Query&) = default;

SafeBrowsingQueryManager::Query::~Query() = default;

bool SafeBrowsingQueryManager::Query::operator<(const Query& other) const {
  return query_id < other.query_id;
}

bool SafeBrowsingQueryManager::Query::IsMainFrame() const {
  return main_frame_item_id == -1;
}

#pragma mark - SafeBrowsingQueryManager::Result

SafeBrowsingQueryManager::Result::Result() = default;

SafeBrowsingQueryManager::Result::Result(const Result&) = default;

SafeBrowsingQueryManager::Result& SafeBrowsingQueryManager::Result::operator=(
    const Result&) = default;

SafeBrowsingQueryManager::Result::~Result() = default;

#pragma mark - SafeBrowsingQueryManager::UrlCheckerClient

SafeBrowsingQueryManager::UrlCheckerClient::UrlCheckerClient() = default;

SafeBrowsingQueryManager::UrlCheckerClient::~UrlCheckerClient() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
}

void SafeBrowsingQueryManager::UrlCheckerClient::CheckUrl(
    std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker,
    const GURL& url,
    const std::string& method,
    base::OnceCallback<void(bool proceed, bool show_error_page)> callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker_ptr =
      url_checker.get();
  active_url_checkers_[std::move(url_checker)] = std::move(callback);
  url_checker_ptr->CheckUrl(url, method,
                            base::BindOnce(&UrlCheckerClient::OnCheckUrlResult,
                                           AsWeakPtr(), url_checker_ptr));
}

void SafeBrowsingQueryManager::UrlCheckerClient::OnCheckUrlResult(
    safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
    safe_browsing::SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier*
        slow_check_notifier,
    bool proceed,
    bool showed_interstitial) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(url_checker);
  if (slow_check_notifier) {
    *slow_check_notifier = base::BindOnce(&UrlCheckerClient::OnCheckComplete,
                                          AsWeakPtr(), url_checker);
    return;
  }

  OnCheckComplete(url_checker, proceed, showed_interstitial);
}

void SafeBrowsingQueryManager::UrlCheckerClient::OnCheckComplete(
    safe_browsing::SafeBrowsingUrlCheckerImpl* url_checker,
    bool proceed,
    bool showed_interstitial) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(url_checker);

  auto it = active_url_checkers_.find(url_checker);
  base::PostTask(
      FROM_HERE, {web::WebThread::UI},
      base::BindOnce(std::move(it->second), proceed, showed_interstitial));

  active_url_checkers_.erase(it);
}
