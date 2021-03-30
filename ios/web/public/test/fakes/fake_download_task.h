// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#import "ios/web/public/download/download_task.h"
#include "url/gurl.h"

namespace web {

// Fake implementation for DownloadTask interface. Can be used for testing.
class FakeDownloadTask : public DownloadTask {
 public:
  FakeDownloadTask(const GURL& original_url, const std::string& mime_type);
  ~FakeDownloadTask() override;

  // DownloadTask overrides:
  WebState* GetWebState() override;
  DownloadTask::State GetState() const override;
  void Start(std::unique_ptr<net::URLFetcherResponseWriter> writer) override;
  void Cancel() override;
  net::URLFetcherResponseWriter* GetResponseWriter() const override;
  NSString* GetIndentifier() const override;
  const GURL& GetOriginalUrl() const override;
  NSString* GetHttpMethod() const override;
  bool IsDone() const override;
  int GetErrorCode() const override;
  int GetHttpCode() const override;
  int64_t GetTotalBytes() const override;
  int64_t GetReceivedBytes() const override;
  int GetPercentComplete() const override;
  std::string GetContentDisposition() const override;
  std::string GetOriginalMimeType() const override;
  std::string GetMimeType() const override;
  std::u16string GetSuggestedFilename() const override;
  bool HasPerformedBackgroundDownload() const override;
  void AddObserver(DownloadTaskObserver* observer) override;
  void RemoveObserver(DownloadTaskObserver* observer) override;

  // Setters for task properties. Setters invoke OnDownloadUpdated callback.
  void SetWebState(WebState* web_state);
  void SetDone(bool done);
  void SetErrorCode(int error_code);
  void SetHttpCode(int http_code);
  void SetTotalBytes(int64_t total_bytes);
  void SetReceivedBytes(int64_t received_bytes);
  void SetPercentComplete(int percent_complete);
  void SetContentDisposition(const std::string& content_disposition);
  void SetMimeType(const std::string& mime_type);
  void SetSuggestedFilename(const std::u16string& suggested_file_name);
  void SetPerformedBackgroundDownload(bool flag);

 private:
  // Called when download task was updated.
  void OnDownloadUpdated();

  base::ObserverList<DownloadTaskObserver, true>::Unchecked observers_;

  WebState* web_state_ = nullptr;
  State state_ = State::kNotStarted;
  std::unique_ptr<net::URLFetcherResponseWriter> writer_;
  GURL original_url_;
  int error_code_ = 0;
  int http_code_ = -1;
  std::string content_disposition_;
  int64_t total_bytes_ = -1;
  int64_t received_bytes_ = 0;
  int percent_complete_ = -1;
  std::string original_mime_type_;
  std::string mime_type_;
  std::u16string suggested_file_name_;
  bool has_performed_background_download_ = false;
  __strong NSString* identifier_ = nil;

  DISALLOW_COPY_AND_ASSIGN(FakeDownloadTask);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_
