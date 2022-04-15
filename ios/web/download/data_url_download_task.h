// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_
#define IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_

#include "base/ios/block_types.h"
#include "ios/web/download/download_task_impl.h"

namespace net {
class URLFetcherResponseWriter;
}

namespace web {

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
                      NSString* identifier,
                      Delegate* delegate);

  DataUrlDownloadTask(const DataUrlDownloadTask&) = delete;
  DataUrlDownloadTask& operator=(const DataUrlDownloadTask&) = delete;

  ~DataUrlDownloadTask() final;

  // DownloadTask overrides:
  NSData* GetResponseData() const final;
  const base::FilePath& GetResponsePath() const final;

  // DownloadTaskImpl overrides:
  void Start(const base::FilePath& path, Destination destination_hint) final;

 private:
  // Called once net::URLFetcherResponseWriter completes the download
  void OnWriterDownloadFinished(int error_code);

  // Called once the net::URLFetcherResponseWriter created in
  // Start() has been initialised. The download can be started
  // unless the initialisation has failed (as reported by the
  // |writer_initialization_status| result).
  void OnWriterInitialized(
      std::unique_ptr<net::URLFetcherResponseWriter> writer,
      int writer_initialization_status);

  // Starts parsing data:// url. Separate code path is used because
  // NSURLSession does not support data URLs.
  void StartDataUrlParsing();

  // Called when data:// url parsing has completed and the data has been
  // written.
  void OnDataUrlWritten(int bytes_written);

  std::unique_ptr<net::URLFetcherResponseWriter> writer_;

  base::WeakPtrFactory<DataUrlDownloadTask> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DATA_URL_DOWNLOAD_TASK_H_
