// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/http_cache_data_remover.h"

#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/data_remover_util.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

HttpCacheDataRemover::HttpCacheDataRemover(
    mojom::ClearDataFilterPtr url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    HttpCacheDataRemoverCallback done_callback)
    : delete_begin_(delete_begin),
      delete_end_(delete_end),
      done_callback_(std::move(done_callback)),
      backend_(nullptr) {
  DCHECK(!done_callback_.is_null());

  if (!url_filter) {
    return;
  }

  url_matcher_ = BindDoesUrlMatchFilter(url_filter->type, url_filter->origins,
                                        url_filter->domains);
}

HttpCacheDataRemover::~HttpCacheDataRemover() = default;

// static.
std::unique_ptr<HttpCacheDataRemover> HttpCacheDataRemover::CreateAndStart(
    net::URLRequestContext* url_request_context,
    mojom::ClearDataFilterPtr url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    HttpCacheDataRemoverCallback done_callback) {
  // Store data from `url_filter` needed by ClearNoVarySearchCache() before we
  // move `url_filter` into HttpCacheDataRemover.
  base::flat_set<url::Origin> origins;
  base::flat_set<std::string> domains;
  // Default to deleting everything in the specified time range if `url_filter`
  // was not supplied.
  net::UrlFilterType url_filter_type = net::UrlFilterType::kFalseIfMatches;
  if (url_filter) {
    const mojom::ClearDataFilter& filter = *url_filter;
    url_filter_type = ConvertClearDataFilterType(filter.type);
    origins = base::flat_set<url::Origin>(filter.origins.begin(),
                                          filter.origins.end());
    domains = base::flat_set<std::string>(filter.domains.begin(),
                                          filter.domains.end());
  }

  DCHECK(done_callback);
  std::unique_ptr<HttpCacheDataRemover> remover(
      new HttpCacheDataRemover(std::move(url_filter), delete_begin, delete_end,
                               std::move(done_callback)));

  net::HttpCache* http_cache =
      url_request_context->http_transaction_factory()->GetCache();
  if (!http_cache) {
    // Some contexts might not have a cache, in which case we are done.
    // Notify by posting a task to avoid reentrency.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpCacheDataRemover::ClearHttpCacheDone,
                       remover->weak_factory_.GetWeakPtr(), net::OK));
    return remover;
  }

  // Clear QUIC server information from memory and the disk cache.
  // TODO(crbug.com/41374118): add a browser test to validate the QUIC
  // information is cleared.
  http_cache->GetSession()
      ->quic_session_pool()
      ->ClearCachedStatesInCryptoConfig(remover->url_matcher_);

  // Clear the in-memory mapping of URLs using the No-Vary-Search response
  // header, and its persisted version on disk.
  http_cache->ClearNoVarySearchCache(url_filter_type, origins, domains,
                                     delete_begin, delete_end);

  auto callback = base::BindOnce(&HttpCacheDataRemover::CacheRetrieved,
                                 remover->weak_factory_.GetWeakPtr());
  net::HttpCache::GetBackendResult result =
      http_cache->GetBackend(std::move(callback));
  if (result.first != net::ERR_IO_PENDING) {
    remover->CacheRetrieved(result);
  }
  return remover;
}

void HttpCacheDataRemover::CacheRetrieved(
    std::pair<int, raw_ptr<disk_cache::Backend>> result) {
  DCHECK(done_callback_);

  int rv = result.first;
  backend_ = result.second;
  // |backend_| can be null if it cannot be initialized.
  if (rv != net::OK || !backend_) {
    backend_ = nullptr;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HttpCacheDataRemover::ClearHttpCacheDone,
                                  weak_factory_.GetWeakPtr(), rv));
    return;
  }

  if (!url_matcher_.is_null()) {
    deletion_helper_ = ConditionalCacheDeletionHelper::CreateAndStart(
        backend_, url_matcher_, delete_begin_, delete_end_,
        base::BindOnce(&HttpCacheDataRemover::ClearHttpCacheDone,
                       weak_factory_.GetWeakPtr(), net::OK));
    return;
  }

  if (delete_begin_.is_null() && delete_end_.is_max()) {
    rv = backend_->DoomAllEntries(base::BindOnce(
        &HttpCacheDataRemover::ClearHttpCacheDone, weak_factory_.GetWeakPtr()));
  } else {
    rv = backend_->DoomEntriesBetween(
        delete_begin_, delete_end_,
        base::BindOnce(&HttpCacheDataRemover::ClearHttpCacheDone,
                       weak_factory_.GetWeakPtr()));
  }
  if (rv != net::ERR_IO_PENDING) {
    // Notify by posting a task to avoid reentrency.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HttpCacheDataRemover::ClearHttpCacheDone,
                                  weak_factory_.GetWeakPtr(), rv));
  }
}

void HttpCacheDataRemover::ClearHttpCacheDone(int rv) {
  std::move(done_callback_).Run(this);
}

}  // namespace network
