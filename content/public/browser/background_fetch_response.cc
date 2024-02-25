// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_fetch_response.h"

namespace content {

BackgroundFetchResponse::BackgroundFetchResponse(
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers)
    : url_chain(url_chain), headers(headers) {}

BackgroundFetchResponse::~BackgroundFetchResponse() = default;

BackgroundFetchResult::BackgroundFetchResult(
    std::unique_ptr<BackgroundFetchResponse> response,
    base::Time response_time,
    FailureReason failure_reason)
    : response(std::move(response)),
      response_time(response_time),
      failure_reason(failure_reason) {}

BackgroundFetchResult::BackgroundFetchResult(
    std::unique_ptr<BackgroundFetchResponse> response,
    base::Time response_time,
    const base::FilePath& path,
    std::optional<storage::BlobDataHandle> blob_handle,
    uint64_t file_size)
    : response(std::move(response)),
      response_time(response_time),
      file_path(path),
      blob_handle(blob_handle),
      file_size(file_size),
      failure_reason(FailureReason::NONE) {}

BackgroundFetchResult::~BackgroundFetchResult() = default;

}  // namespace content
