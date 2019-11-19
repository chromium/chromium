// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_MATCH_PARAMS_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_MATCH_PARAMS_H_

#include "base/optional.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace content {

class CONTENT_EXPORT BackgroundFetchRequestMatchParams {
 public:
  BackgroundFetchRequestMatchParams();
  BackgroundFetchRequestMatchParams(
      blink::mojom::FetchAPIRequestPtr request_to_match,
      blink::mojom::CacheQueryOptionsPtr cache_query_options,
      bool match_all);
  ~BackgroundFetchRequestMatchParams();

  bool FilterByRequest() const { return !request_to_match_.is_null(); }

  // Only call this method if a valid request_to_match was previously provided.
  const blink::mojom::FetchAPIRequestPtr& request_to_match() const {
    DCHECK(request_to_match_);
    return request_to_match_;
  }

  const blink::mojom::CacheQueryOptionsPtr& cache_query_options() const {
    return cache_query_options_;
  }

  blink::mojom::CacheQueryOptionsPtr cloned_cache_query_options() const {
    if (!cache_query_options_)
      return nullptr;
    return cache_query_options_->Clone();
  }

  bool match_all() const { return match_all_; }

 private:
  // If |request_to_match| is present, we get response(s) only for this request.
  // If not present, response(s) for all requests (contained in the fetch) will
  // be returned.
  blink::mojom::FetchAPIRequestPtr request_to_match_;

  // When nullptr, this has no effect on the response(s) returned.
  blink::mojom::CacheQueryOptionsPtr cache_query_options_;

  // Whether to return all matching responses from the cache storage.
  bool match_all_ = false;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchRequestMatchParams);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_MATCH_PARAMS_H_
