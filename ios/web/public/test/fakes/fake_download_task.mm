// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_download_task.h"

#import "base/functional/callback.h"
#import "ios/web/public/download/download_task_observer.h"

namespace web {

FakeDownloadTask::FakeDownloadTask(const GURL& original_url,
                                   const std::string& mime_type)
    : original_url_(original_url),
      original_mime_type_(mime_type),
      mime_type_(mime_type),
      identifier_(@"") {}

FakeDownloadTask::~FakeDownloadTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);
}

WebState* FakeDownloadTask::GetWebState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_state_;
}

DownloadTask::State FakeDownloadTask::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

void FakeDownloadTask::Start(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  response_data_ = nil;
  response_path_ = path;
  state_ = State::kInProgress;
  OnDownloadUpdated();
}

void FakeDownloadTask::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kCancelled;
  OnDownloadUpdated();
}

void FakeDownloadTask::GetResponseData(
    ResponseDataReadCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(response_data_);
}

const base::FilePath& FakeDownloadTask::GetResponsePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_path_;
}

NSString* FakeDownloadTask::GetIdentifier() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return identifier_;
}

const GURL& FakeDownloadTask::GetOriginalUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return original_url_;
}

NSString* FakeDownloadTask::GetHttpMethod() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return @"GET";
}

bool FakeDownloadTask::IsDone() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == State::kComplete;
}

int FakeDownloadTask::GetErrorCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return error_code_;
}

int FakeDownloadTask::GetHttpCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_code_;
}

int64_t FakeDownloadTask::GetTotalBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return total_bytes_;
}

int64_t FakeDownloadTask::GetReceivedBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return received_bytes_;
}

int FakeDownloadTask::GetPercentComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return percent_complete_;
}

std::string FakeDownloadTask::GetContentDisposition() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_disposition_;
}

std::string FakeDownloadTask::GetOriginalMimeType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return original_mime_type_;
}

std::string FakeDownloadTask::GetMimeType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mime_type_;
}

base::FilePath FakeDownloadTask::GenerateFileName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return generated_file_name_;
}

bool FakeDownloadTask::HasPerformedBackgroundDownload() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_performed_background_download_;
}

void FakeDownloadTask::AddObserver(DownloadTaskObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void FakeDownloadTask::RemoveObserver(DownloadTaskObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void FakeDownloadTask::SetWebState(WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web_state_ = web_state;
}

void FakeDownloadTask::SetState(DownloadTask::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = state;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetDone(bool done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response_data_) {
    response_data_ = [NSData data];
  }
  state_ = State::kComplete;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetErrorCode(int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  error_code_ = error_code;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetHttpCode(int http_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  http_code_ = http_code;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetTotalBytes(int64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  total_bytes_ = total_bytes;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetReceivedBytes(int64_t received_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  received_bytes_ = received_bytes;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetResponseData(NSData* received_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  response_data_ = received_data;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetPercentComplete(int percent_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  percent_complete_ = percent_complete;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetContentDisposition(
    const std::string& content_disposition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content_disposition_ = content_disposition;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetMimeType(const std::string& mime_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mime_type_ = mime_type;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetGeneratedFileName(
    const base::FilePath& generated_file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  generated_file_name_ = generated_file_name;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetPerformedBackgroundDownload(bool flag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_performed_background_download_ = flag;
}

void FakeDownloadTask::OnDownloadUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

}  // namespace web
