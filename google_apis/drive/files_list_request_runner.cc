// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/files_list_request_runner.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/drive/drive_api_requests.h"

namespace google_apis {

FilesListRequestRunner::FilesListRequestRunner(
    RequestSender* request_sender,
    const google_apis::DriveApiUrlGenerator& url_generator)
    : request_sender_(request_sender), url_generator_(url_generator) {}

FilesListRequestRunner::~FilesListRequestRunner() = default;

CancelCallbackOnce FilesListRequestRunner::CreateAndStartWithSizeBackoff(
    int max_results,
    FilesListCorpora corpora,
    const std::string& team_drive_id,
    const std::string& q,
    const std::string& fields,
    FileListCallback callback) {
  base::OnceClosure* cancel_callback = new base::OnceClosure;
  std::unique_ptr<drive::FilesListRequest> request =
      std::make_unique<drive::FilesListRequest>(
          request_sender_, url_generator_,
          base::BindOnce(&FilesListRequestRunner::OnCompleted,
                         weak_ptr_factory_.GetWeakPtr(), max_results, corpora,
                         team_drive_id, q, fields, std::move(callback),
                         base::Owned(cancel_callback)));
  request->set_max_results(max_results);
  request->set_q(q);
  request->set_fields(fields);
  *cancel_callback =
      request_sender_->StartRequestWithAuthRetry(std::move(request));

  // The cancellation callback is owned by the completion callback, so it must
  // not be used after |callback| is called.
  return base::BindOnce(&FilesListRequestRunner::OnCancel,
                        weak_ptr_factory_.GetWeakPtr(),
                        base::Unretained(cancel_callback));
}

void FilesListRequestRunner::OnCancel(CancelCallbackOnce* cancel_callback) {
  DCHECK(cancel_callback);
  DCHECK(!cancel_callback->is_null());
  std::move(*cancel_callback).Run();
}

void FilesListRequestRunner::OnCompleted(int max_results,
                                         FilesListCorpora corpora,
                                         const std::string& team_drive_id,
                                         const std::string& q,
                                         const std::string& fields,
                                         FileListCallback callback,
                                         CancelCallbackOnce* cancel_callback,
                                         ApiErrorCode error,
                                         std::unique_ptr<FileList> entry) {
  if (!request_completed_callback_for_testing_.is_null())
    std::move(request_completed_callback_for_testing_).Run();

  if (error == google_apis::DRIVE_RESPONSE_TOO_LARGE && max_results > 1) {
    CreateAndStartWithSizeBackoff(max_results / 2, corpora, team_drive_id, q,
                                  fields, std::move(callback));
    return;
  }

  std::move(callback).Run(error, std::move(entry));
}

void FilesListRequestRunner::SetRequestCompletedCallbackForTesting(
    base::OnceClosure callback) {
  request_completed_callback_for_testing_ = std::move(callback);
}

}  // namespace google_apis
