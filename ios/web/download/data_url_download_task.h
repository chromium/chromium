// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_
#define IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_

#include "ios/web/download/download_task_impl.h"

namespace web {
namespace download {
namespace internal {
struct ParseDataUrlResult;
}  // namespace internal
}  // namespace download

// Implementation of DownloadTaskImpl that uses NSURLRequest to perform the
// download.
class DataUrlDownloadTask final : public DownloadTaskImpl {
 public:
  // Constructs a new DataUrlDownloadTask objects. `web_state` and `identifier`
  // must be valid.
  DataUrlDownloadTask(
      WebState* web_state,
      const GURL& original_url,
      NSString* http_method,
      const std::string& content_disposition,
      int64_t total_bytes,
      const std::string& mime_type,
      NSString* identifier,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  DataUrlDownloadTask(const DataUrlDownloadTask&) = delete;
  DataUrlDownloadTask& operator=(const DataUrlDownloadTask&) = delete;

  ~DataUrlDownloadTask() final;

  // DownloadTaskImpl overrides:
  void StartInternal(const base::FilePath& path) final;
  void CancelInternal() final;

 private:
  // Called when the data: url has been parsed and optionally written to disk.
  void OnDataUrlParsed(download::internal::ParseDataUrlResult result);

  base::WeakPtrFactory<DataUrlDownloadTask> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_
