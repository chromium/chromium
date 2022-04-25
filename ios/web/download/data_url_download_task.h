// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_
#define IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_

#include "ios/web/download/download_task_impl.h"

namespace web {
namespace internal {
struct ParseDataUrlResult;
}  // namespace internal

// Implementation of DownloadTaskImpl that uses NSURLRequest to perform the
// download.
class DataUrlDownloadTask final : public DownloadTaskImpl {
 public:
  // Constructs a new DataUrlDownloadTask objects. |web_state|, |identifier|
  // and |delegate| must be valid.
  DataUrlDownloadTask(WebState* web_state,
                      const GURL& original_url,
                      NSString* http_method,
                      const std::string& content_disposition,
                      int64_t total_bytes,
                      const std::string& mime_type,
                      NSString* identifier);

  DataUrlDownloadTask(const DataUrlDownloadTask&) = delete;
  DataUrlDownloadTask& operator=(const DataUrlDownloadTask&) = delete;

  ~DataUrlDownloadTask() final;

  // DownloadTask overrides:
  NSData* GetResponseData() const final;
  const base::FilePath& GetResponsePath() const final;

  // DownloadTaskImpl overrides:
  void Start(const base::FilePath& path, Destination destination_hint) final;

 private:
  // Called when the data: url has been parsed and optionally written to disk.
  void OnDataUrlParsed(internal::ParseDataUrlResult result);

  __strong NSData* data_ = nil;
  base::FilePath path_;

  base::WeakPtrFactory<DataUrlDownloadTask> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_
