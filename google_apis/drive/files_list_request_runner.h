// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_FILES_LIST_REQUEST_RUNNER_H_
#define GOOGLE_APIS_DRIVE_FILES_LIST_REQUEST_RUNNER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/drive/drive_api_requests.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/drive/drive_common_callbacks.h"

namespace google_apis {

class RequestSender;

// Runs file list requests (the FileListRequest class) with a backoff retry
// logic in case of the DRIVE_RESPONSE_TOO_LARGE error code.
class FilesListRequestRunner {
 public:
  FilesListRequestRunner(
      RequestSender* request_sender,
      const google_apis::DriveApiUrlGenerator& url_generator);

  // Creates a FilesListRequest instance and starts the request with a backoff
  // retry in case of DRIVE_RESPONSE_TOO_LARGE error code.
  CancelCallback CreateAndStartWithSizeBackoff(
      int max_results,
      FilesListCorpora corpora,
      const std::string& team_drive_id,
      const std::string& q,
      const std::string& fields,
      const FileListCallback& callback);

  ~FilesListRequestRunner();

  void SetRequestCompletedCallbackForTesting(const base::Closure& callback);

 private:
  // Called when the cancelling callback returned by
  // CreateAndStartWithSizeBackoff is invoked. Once called cancels the current
  // request.
  void OnCancel(CancelCallback* cancel_callback);

  // Called when a single request is completed with either a success or an
  // error. In case of DRIVE_RESPONSE_TOO_LARGE it will retry the request with
  // half of the requests.
  void OnCompleted(int max_results,
                   FilesListCorpora corpora,
                   const std::string& team_drive_id,
                   const std::string& q,
                   const std::string& fields,
                   const FileListCallback& callback,
                   CancelCallback* cancel_callback,
                   DriveApiErrorCode error,
                   std::unique_ptr<FileList> entry);

  RequestSender* request_sender_;                          // Not owned.
  const google_apis::DriveApiUrlGenerator url_generator_;  // Not owned.
  base::Closure request_completed_callback_for_testing_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FilesListRequestRunner> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FilesListRequestRunner);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_FILES_LIST_REQUEST_RUNNER_H_
