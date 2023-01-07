// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_INFO_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_INFO_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace content {

struct BackgroundFetchResponse;
struct BackgroundFetchResult;
class ChromeBlobStorageContext;

// Class to encapsulate the components of a fetch request.
class CONTENT_EXPORT BackgroundFetchRequestInfo
    : public base::RefCountedDeleteOnSequence<BackgroundFetchRequestInfo> {
 public:
  BackgroundFetchRequestInfo(int request_index,
                             blink::mojom::FetchAPIRequestPtr fetch_request,
                             uint64_t request_body_size);

  BackgroundFetchRequestInfo(const BackgroundFetchRequestInfo&) = delete;
  BackgroundFetchRequestInfo& operator=(const BackgroundFetchRequestInfo&) =
      delete;

  // Sets the download GUID to a newly generated value. Can only be used if no
  // GUID is already set.
  void InitializeDownloadGuid();

  // Sets the download GUID to a given value (to be used when requests are
  // retrieved from storage). Can only be used if no GUID is already set.
  void SetDownloadGuid(const std::string& download_guid);

  // Extracts the headers and the status code.
  void PopulateWithResponse(std::unique_ptr<BackgroundFetchResponse> response);

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
  const blink::mojom::FetchAPIRequestPtr& fetch_request() const {
    return fetch_request_;
  }

  // Returns the Fetch API Request Ptr object that details the developer's
  // request.
  const blink::mojom::FetchAPIRequestPtr& fetch_request_ptr() const {
    return fetch_request_;
  }

  // Returns the size of the blob to upload.
  uint64_t request_body_size() const { return request_body_size_; }

  void set_can_populate_body(bool can_populate_body) {
    can_populate_body_ = can_populate_body;
  }

  bool can_populate_body() const { return can_populate_body_; }

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

  // Creates a blob data handle for the response.
  void CreateResponseBlobDataHandle(
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context);

  // Hands over ownership of the blob data handle. Called on the IO thread.
  //
  // This must be called after `CreateResponseBlobDataHandle` has been called
  // and a task that it posted to the IO thread has run.
  std::unique_ptr<storage::BlobDataHandle> TakeResponseBlobDataHandleOnIO();

  // Returns the size of the response.
  uint64_t GetResponseSize() const;

  // Returns the time at which the response was completed.
  const base::Time& GetResponseTime() const;

  // Whether the BackgroundFetchResult was successful.
  bool IsResultSuccess() const;

 private:
  class BlobDataOnIO;
  friend class base::RefCountedDeleteOnSequence<BackgroundFetchRequestInfo>;
  friend class base::DeleteHelper<BackgroundFetchRequestInfo>;
  friend class BackgroundFetchCrossOriginFilterTest;

  ~BackgroundFetchRequestInfo();

  // ---- Data associated with the request -------------------------------------
  int request_index_ = kInvalidBackgroundFetchRequestIndex;
  blink::mojom::FetchAPIRequestPtr fetch_request_;
  uint64_t request_body_size_;

  // ---- Data associated with the in-progress download ------------------------
  std::string download_guid_;
  download::DownloadItem::DownloadState download_state_ =
      download::DownloadItem::IN_PROGRESS;

  int response_code_ = 0;
  std::string response_text_;
  std::map<std::string, std::string> response_headers_;
  std::vector<GURL> url_chain_;
  bool can_populate_body_ = false;

  // ---- Data associated with the response ------------------------------------
  std::unique_ptr<BackgroundFetchResult> result_;
  // Created on this class's sequence, then accessed on the IO thread only.
  std::unique_ptr<BlobDataOnIO, BrowserThread::DeleteOnIOThread> io_blob_data_;
  uint64_t response_size_ = 0u;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REQUEST_INFO_H_
