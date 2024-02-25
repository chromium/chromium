// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_

#include <string>

#include "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/web/public/download/download_task.h"
#include "url/gurl.h"

namespace web {

// Fake implementation for DownloadTask interface. Can be used for testing.
class FakeDownloadTask final : public DownloadTask {
 public:
  FakeDownloadTask(const GURL& original_url, const std::string& mime_type);

  FakeDownloadTask(const FakeDownloadTask&) = delete;
  FakeDownloadTask& operator=(const FakeDownloadTask&) = delete;

  ~FakeDownloadTask() final;

  // DownloadTask finals:
  WebState* GetWebState() final;
  DownloadTask::State GetState() const final;
  void Start(const base::FilePath& path) final;
  void Cancel() final;
  void GetResponseData(ResponseDataReadCallback callback) const final;
  const base::FilePath& GetResponsePath() const final;
  NSString* GetIdentifier() const final;
  const GURL& GetOriginalUrl() const final;
  NSString* GetHttpMethod() const final;
  bool IsDone() const final;
  int GetErrorCode() const final;
  int GetHttpCode() const final;
  int64_t GetTotalBytes() const final;
  int64_t GetReceivedBytes() const final;
  int GetPercentComplete() const final;
  std::string GetContentDisposition() const final;
  std::string GetOriginalMimeType() const final;
  std::string GetMimeType() const final;
  base::FilePath GenerateFileName() const final;
  bool HasPerformedBackgroundDownload() const final;
  void AddObserver(DownloadTaskObserver* observer) final;
  void RemoveObserver(DownloadTaskObserver* observer) final;

  // Setters for task properties. Setters invoke OnDownloadUpdated callback.
  void SetWebState(WebState* web_state);
  void SetState(DownloadTask::State state);
  void SetDone(bool done);
  void SetErrorCode(int error_code);
  void SetHttpCode(int http_code);
  void SetTotalBytes(int64_t total_bytes);
  void SetReceivedBytes(int64_t received_bytes);
  void SetPercentComplete(int percent_complete);
  void SetContentDisposition(const std::string& content_disposition);
  void SetMimeType(const std::string& mime_type);
  void SetResponseData(NSData* response_data);
  void SetGeneratedFileName(const base::FilePath& generated_file_name);
  void SetPerformedBackgroundDownload(bool flag);

 private:
  // Called when download task was updated.
  void OnDownloadUpdated();

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<DownloadTaskObserver, true> observers_;
  raw_ptr<WebState> web_state_ = nullptr;
  State state_ = State::kNotStarted;
  GURL original_url_;
  int error_code_ = 0;
  int http_code_ = -1;
  std::string content_disposition_;
  int64_t total_bytes_ = -1;
  int64_t received_bytes_ = 0;
  int percent_complete_ = -1;
  std::string original_mime_type_;
  std::string mime_type_;
  base::FilePath generated_file_name_;
  bool has_performed_background_download_ = false;
  __strong NSString* identifier_ = nil;
  base::FilePath response_path_;
  __strong NSData* response_data_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_
