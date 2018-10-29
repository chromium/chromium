// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_INFO_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_INFO_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/background_fetch_response.h"
#include "url/gurl.h"

namespace storage {
class BlobDataHandle;
}

namespace content {

struct BackgroundFetchResponse;
struct BackgroundFetchResult;

// Simple class to encapsulate the components of a fetch request.
// TODO(peter): This can likely change to have a single owner, and thus become
// an std::unique_ptr<>, when persistent storage has been implemented.
class CONTENT_EXPORT BackgroundFetchRequestInfo
    : public base::RefCountedDeleteOnSequence<BackgroundFetchRequestInfo> {
 public:
  BackgroundFetchRequestInfo(int request_index,
                             const ServiceWorkerFetchRequest& fetch_request);

  // Sets the download GUID to a newly generated value. Can only be used if no
  // GUID is already set.
  void InitializeDownloadGuid();

  // Sets the download GUID to a given value (to be used when requests are
  // retrieved from storage). Can only be used if no GUID is already set.
  void SetDownloadGuid(const std::string& download_guid);

  void SetResult(std::unique_ptr<BackgroundFetchResult> result);

  // Creates an empty result, with no response, and assigns |failure_reason|
  // as its failure_reason.
  void SetEmptyResultWithFailureReason(
      BackgroundFetchResult::FailureReason failure_reason);

  // Returns the index of this request within a Background Fetch registration.
  int request_index() const { return request_index_; }

  // Returns the GUID used to identify this download. (Empty before the download
  // becomes active).
  const std::string& download_guid() const { return download_guid_; }

  // Returns the Fetch API Request object that details the developer's request.
  const ServiceWorkerFetchRequest& fetch_request() const {
    return fetch_request_;
  }

  // Returns the response code for the download. Available for both successful
  // and failed requests.
  int GetResponseCode() const;

  // Returns the response text for the download. Available for all started
  // items.
  const std::string& GetResponseText() const;

  // Returns the response headers for the download. Available for both
  // successful and failed requests.
  const std::map<std::string, std::string>& GetResponseHeaders() const;

  // Returns the URL chain for the response, including redirects.
  const std::vector<GURL>& GetURLChain() const;

  // Returns the blob data handle for the response. Only available when dealing
  // with in-memory downloads.
  const base::Optional<storage::BlobDataHandle>& GetBlobDataHandle() const;

  // Returns the absolute path to the file in which the response is stored.
  const base::FilePath& GetFilePath() const;

  // Returns the size of the file containing the response, in bytes.
  int64_t GetFileSize() const;

  // Returns the time at which the response was completed.
  const base::Time& GetResponseTime() const;

  // Whether the BackgroundFetchResult was successful.
  bool IsResultSuccess() const;

 private:
  friend class base::RefCountedDeleteOnSequence<BackgroundFetchRequestInfo>;
  friend class base::DeleteHelper<BackgroundFetchRequestInfo>;
  friend class BackgroundFetchCrossOriginFilterTest;

  // Extracts the headers and the status code.
  void PopulateWithResponse(std::unique_ptr<BackgroundFetchResponse> response);

  ~BackgroundFetchRequestInfo();

  // ---- Data associated with the request -------------------------------------
  int request_index_ = kInvalidBackgroundFetchRequestIndex;
  ServiceWorkerFetchRequest fetch_request_;

  // ---- Data associated with the in-progress download ------------------------
  std::string download_guid_;
  download::DownloadItem::DownloadState download_state_ =
      download::DownloadItem::IN_PROGRESS;

  int response_code_ = 0;
  std::string response_text_;
  std::map<std::string, std::string> response_headers_;
  std::vector<GURL> url_chain_;

  // ---- Data associated with the response ------------------------------------
  std::unique_ptr<BackgroundFetchResult> result_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchRequestInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_INFO_H_
