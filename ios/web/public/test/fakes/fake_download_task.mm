// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_download_task.h"

#include "ios/web/public/download/download_task_observer.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeDownloadTask::FakeDownloadTask(const GURL& original_url,
                                   const std::string& mime_type)
    : original_url_(original_url),
      original_mime_type_(mime_type),
      mime_type_(mime_type),
      identifier_(@"") {}

FakeDownloadTask::~FakeDownloadTask() {
  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);
}

WebState* FakeDownloadTask::GetWebState() {
  return web_state_;
}

DownloadTask::State FakeDownloadTask::GetState() const {
  return state_;
}

void FakeDownloadTask::Start(
    std::unique_ptr<net::URLFetcherResponseWriter> writer) {
  writer_ = std::move(writer);
  state_ = State::kInProgress;
  OnDownloadUpdated();
}

void FakeDownloadTask::Cancel() {
  state_ = State::kCancelled;
  OnDownloadUpdated();
}

net::URLFetcherResponseWriter* FakeDownloadTask::GetResponseWriter() const {
  return writer_.get();
}

NSString* FakeDownloadTask::GetIndentifier() const {
  return identifier_;
}

const GURL& FakeDownloadTask::GetOriginalUrl() const {
  return original_url_;
}

NSString* FakeDownloadTask::GetHttpMethod() const {
  return @"GET";
}

bool FakeDownloadTask::IsDone() const {
  return state_ == State::kComplete;
}

int FakeDownloadTask::GetErrorCode() const {
  return error_code_;
}

int FakeDownloadTask::GetHttpCode() const {
  return http_code_;
}

int64_t FakeDownloadTask::GetTotalBytes() const {
  return total_bytes_;
}

int64_t FakeDownloadTask::GetReceivedBytes() const {
  return received_bytes_;
}

int FakeDownloadTask::GetPercentComplete() const {
  return percent_complete_;
}

std::string FakeDownloadTask::GetContentDisposition() const {
  return content_disposition_;
}

std::string FakeDownloadTask::GetOriginalMimeType() const {
  return original_mime_type_;
}

std::string FakeDownloadTask::GetMimeType() const {
  return mime_type_;
}

std::u16string FakeDownloadTask::GetSuggestedFilename() const {
  return suggested_file_name_;
}

bool FakeDownloadTask::HasPerformedBackgroundDownload() const {
  return has_performed_background_download_;
}

void FakeDownloadTask::AddObserver(DownloadTaskObserver* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void FakeDownloadTask::RemoveObserver(DownloadTaskObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void FakeDownloadTask::SetWebState(WebState* web_state) {
  web_state_ = web_state;
}

void FakeDownloadTask::SetDone(bool done) {
  state_ = State::kComplete;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetErrorCode(int error_code) {
  error_code_ = error_code;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetHttpCode(int http_code) {
  http_code_ = http_code;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetTotalBytes(int64_t total_bytes) {
  total_bytes_ = total_bytes;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetReceivedBytes(int64_t received_bytes) {
  received_bytes_ = received_bytes;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetPercentComplete(int percent_complete) {
  percent_complete_ = percent_complete;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetContentDisposition(
    const std::string& content_disposition) {
  content_disposition_ = content_disposition;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetMimeType(const std::string& mime_type) {
  mime_type_ = mime_type;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetSuggestedFilename(
    const std::u16string& suggested_file_name) {
  suggested_file_name_ = suggested_file_name;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetPerformedBackgroundDownload(bool flag) {
  has_performed_background_download_ = flag;
}

void FakeDownloadTask::OnDownloadUpdated() {
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

}  // namespace web
