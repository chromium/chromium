// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_FETCHER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_FETCHER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_update_job.h"
#include "net/base/io_buffer.h"
#include "url/gurl.h"

namespace content {

// Fetches a manifest or a referenced resource.
//
// The manifest is fetched into an in-memory string, and can be read using
// manifest_data(). Other resources are stored in net::disk_cache entries.
class AppCacheUpdateJob::URLFetcher {
 public:
  enum class FetchType {
    // Fetching an URL that is explicitly called out in the manifest. When
    // updating a manifest, this type is also used for HTML pages (master
    // entries) that have already been found to point to the manifest.
    kResource,
    // Fetching a HTML page pointing to a manifest that is already cached. The
    // page will be added as a master entry to the existing cache.
    kNewMasterEntry,
    kManifest,
    kManifestRefetch,
  };

  // The caller must ensure that |job| outlives this instance.
  URLFetcher(const GURL& url,
             FetchType fetch_type,
             AppCacheUpdateJob* job,
             int buffer_size);
  ~URLFetcher();

  void Start();

  FetchType fetch_type() const { return fetch_type_; }

  const AppCacheEntry& existing_entry() const { return existing_entry_; }
  void set_existing_entry(const AppCacheEntry& entry) {
    existing_entry_ = entry;
  }
  void set_existing_response_headers(
      scoped_refptr<net::HttpResponseHeaders> headers) {
    existing_response_headers_ = std::move(headers);
  }

  UpdateURLLoaderRequest* request() const { return request_.get(); }

  // Only valid when fetching a manifest.
  const std::string& manifest_data() const {
    DCHECK(fetch_type_ == FetchType::kManifest ||
           fetch_type_ == FetchType::kManifestRefetch);
    return manifest_data_;
  }

  // Only valid when fetching a resource or a new master entry.
  AppCacheResponseWriter* response_writer() const {
    DCHECK(fetch_type_ == FetchType::kResource ||
           fetch_type_ == FetchType::kNewMasterEntry);
    return response_writer_.get();
  }

  AppCacheUpdateJob::ResultType result() const { return result_; }
  int redirect_response_code() const { return redirect_response_code_; }

 private:
  void OnReceivedRedirect(const net::RedirectInfo& redirect_info);
  void OnResponseStarted(int net_error);
  void OnReadCompleted(net::IOBuffer* buffer, int bytes_read);

  void AddConditionalHeaders(const net::HttpResponseHeaders* headers);
  void OnWriteComplete(int result);
  void ReadResponseData();
  bool ConsumeResponseData(net::IOBuffer* buffer, int bytes_read);
  void OnResponseCompleted(int net_error);
  bool MaybeRetryRequest();

  friend class UpdateURLLoaderRequest;

  const GURL url_;
  AppCacheUpdateJob* const job_;
  const FetchType fetch_type_;
  const int buffer_size_;

  AppCacheEntry existing_entry_;
  scoped_refptr<net::HttpResponseHeaders> existing_response_headers_;

  std::unique_ptr<AppCacheResponseWriter> response_writer_;
  AppCacheUpdateJob::ResultType result_ = AppCacheUpdateJob::UPDATE_OK;

  // Populated after successfully fetching a manifest.
  std::string manifest_data_;

  // Never null. Only modified when the fetch is retried.
  std::unique_ptr<UpdateURLLoaderRequest> request_;

  // Number of times the fetch has been retried.
  //
  // When receiving a 503 response, we re-attempt fetching the resource for a
  // few times.
  int retry_count_ = 0;

  int redirect_response_code_ = -1;
};  // class URLFetcher

}  // namespace content.

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_FETCHER_H_
