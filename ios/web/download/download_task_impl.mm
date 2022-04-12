// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_task_impl.h"

#import <WebKit/WebKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/web/download/download_result.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state.h"
#import "net/base/filename_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

DownloadTaskImpl::DownloadTaskImpl(WebState* web_state,
                                   const GURL& original_url,
                                   NSString* http_method,
                                   const std::string& content_disposition,
                                   int64_t total_bytes,
                                   const std::string& mime_type,
                                   NSString* identifier,
                                   Delegate* delegate)
    : original_url_(original_url),
      http_method_(http_method),
      total_bytes_(total_bytes),
      content_disposition_(content_disposition),
      original_mime_type_(mime_type),
      mime_type_(mime_type),
      identifier_([identifier copy]),
      web_state_(web_state),
      delegate_(delegate) {
  DCHECK(web_state_);
  DCHECK(delegate_);
  base::WeakPtr<DownloadTaskImpl> weak_Task = weak_factory_.GetWeakPtr();
  observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIApplicationWillResignActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* _Nonnull) {
                DownloadTaskImpl* task = weak_Task.get();
                if (task) {
                  if (task->state_ == State::kInProgress) {
                    task->has_performed_background_download_ = true;
                  }
                }
              }];
}

DownloadTaskImpl::~DownloadTaskImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [NSNotificationCenter.defaultCenter removeObserver:observer_];
  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);

  if (delegate_) {
    delegate_->OnTaskDestroyed(this);
    delegate_ = nullptr;
  }
}

void DownloadTaskImpl::ShutDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = nullptr;
}

WebState* DownloadTaskImpl::GetWebState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_state_;
}

DownloadTask::State DownloadTaskImpl::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

void DownloadTaskImpl::Start(const base::FilePath& path,
                             Destination destination_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path != base::FilePath() ||
         destination_hint == DownloadTask::Destination::kToMemory);
  DCHECK_NE(state_, State::kInProgress);
  state_ = State::kInProgress;
  percent_complete_ = 0;
  received_bytes_ = 0;
}

void DownloadTaskImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kCancelled;
  OnDownloadUpdated();
}

NSString* DownloadTaskImpl::GetIndentifier() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return identifier_;
}

const GURL& DownloadTaskImpl::GetOriginalUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return original_url_;
}

NSString* DownloadTaskImpl::GetHttpMethod() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_method_;
}

bool DownloadTaskImpl::IsDone() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::kNotStarted:
    case State::kInProgress:
      return false;
    case State::kCancelled:
    case State::kComplete:
    case State::kFailed:
    case State::kFailedNotResumable:
      return true;
  }
}

int DownloadTaskImpl::GetErrorCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return download_result_.error_code();
}

int DownloadTaskImpl::GetHttpCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_code_;
}

int64_t DownloadTaskImpl::GetTotalBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return total_bytes_;
}

int64_t DownloadTaskImpl::GetReceivedBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return received_bytes_;
}

int DownloadTaskImpl::GetPercentComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return percent_complete_;
}

std::string DownloadTaskImpl::GetContentDisposition() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_disposition_;
}

std::string DownloadTaskImpl::GetOriginalMimeType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return original_mime_type_;
}

std::string DownloadTaskImpl::GetMimeType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mime_type_;
}

std::u16string DownloadTaskImpl::GetSuggestedFilename() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return net::GetSuggestedFilename(GetOriginalUrl(), GetContentDisposition(),
                                   /*referrer_charset=*/std::string(),
                                   /*suggested_name=*/std::string(),
                                   /*mime_type=*/std::string(),
                                   /*default_name=*/"document");
}

bool DownloadTaskImpl::HasPerformedBackgroundDownload() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_performed_background_download_;
}

void DownloadTaskImpl::AddObserver(DownloadTaskObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void DownloadTaskImpl::RemoveObserver(DownloadTaskObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void DownloadTaskImpl::OnDownloadUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

void DownloadTaskImpl::OnDownloadFinished(DownloadResult download_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  download_result_ = download_result;
  if (download_result_.error_code()) {
    state_ = download_result_.can_retry() ? State::kFailed
                                          : State::kFailedNotResumable;
  } else {
    state_ = State::kComplete;
  }

  OnDownloadUpdated();
}

}  // namespace web
