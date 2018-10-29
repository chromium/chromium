// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_RESPONSE_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_RESPONSE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "net/http/http_response_headers.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "url/gurl.h"

namespace content {

// Contains the response after a background fetch has started.
struct CONTENT_EXPORT BackgroundFetchResponse {
  BackgroundFetchResponse(
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers);

  ~BackgroundFetchResponse();

  const std::vector<GURL> url_chain;
  const scoped_refptr<const net::HttpResponseHeaders> headers;  // May be null.

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchResponse);
};

struct CONTENT_EXPORT BackgroundFetchResult {
  // Failures that happen after the download has already started and are
  // reported via |BackgroundFetchDelegate::Client::OnDownloadComplete|.
  enum class FailureReason {
    // None of below failures occurred, although the fetch could still have
    // failed with an error code such as 404.
    NONE,

    // Used when the download has been aborted after reaching a threshold where
    // it was decided it is not worth attempting to start again. This could be
    // either due to a specific number of failed retry attempts or a specific
    // number of wasted bytes due to the download restarting.
    NETWORK,

    // Used when the download was not completed before the timeout.
    TIMEDOUT,

    // Used when the download was cancelled by the user.
    CANCELLED,

    // Catch-all error. Used when the failure reason is unknown or not exposed
    // to the developer.
    FETCH_ERROR,
  };

  // Constructor for failed downloads.
  BackgroundFetchResult(std::unique_ptr<BackgroundFetchResponse> response,
                        base::Time response_time,
                        FailureReason failure_reason);

  // Constructor for successful downloads.
  BackgroundFetchResult(std::unique_ptr<BackgroundFetchResponse> response,
                        base::Time response_time,
                        const base::FilePath& path,
                        base::Optional<storage::BlobDataHandle> blob_handle,
                        uint64_t file_size);

  ~BackgroundFetchResult();

  std::unique_ptr<BackgroundFetchResponse> response;
  const base::Time response_time;
  const base::FilePath file_path;
  base::Optional<storage::BlobDataHandle> blob_handle;
  const uint64_t file_size = 0;
  FailureReason failure_reason;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchResult);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_FETCH_RESPONSE_H_
