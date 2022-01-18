// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_task_impl.h"

#import <WebKit/WebKit.h>

#include "base/strings/sys_string_conversions.h"
#include "ios/web/download/download_result.h"
#import "ios/web/public/download/download_task_observer.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#include "net/base/filename_util.h"

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
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
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
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [NSNotificationCenter.defaultCenter removeObserver:observer_];
  for (auto& observer : observers_)
    observer.OnDownloadDestroyed(this);

  if (delegate_) {
    delegate_->OnTaskDestroyed(this);
    delegate_ = nullptr;
  }
}

void DownloadTaskImpl::ShutDown() {
  delegate_ = nullptr;
}

WebState* DownloadTaskImpl::GetWebState() {
  return web_state_;
}

DownloadTask::State DownloadTaskImpl::GetState() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return state_;
}

void DownloadTaskImpl::Start(const base::FilePath& path,
                             Destination destination_hint) {
  DCHECK(path != base::FilePath() ||
         destination_hint == DownloadTask::Destination::kToMemory);
  DCHECK_NE(state_, State::kInProgress);
  state_ = State::kInProgress;
  percent_complete_ = 0;
  received_bytes_ = 0;
}

void DownloadTaskImpl::Cancel() {
  state_ = State::kCancelled;
  OnDownloadUpdated();
}

NSString* DownloadTaskImpl::GetIndentifier() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return identifier_;
}

const GURL& DownloadTaskImpl::GetOriginalUrl() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return original_url_;
}

NSString* DownloadTaskImpl::GetHttpMethod() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return http_method_;
}

bool DownloadTaskImpl::IsDone() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return state_ == State::kComplete || state_ == State::kCancelled;
}

int DownloadTaskImpl::GetErrorCode() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return download_result_.error_code();
}

int DownloadTaskImpl::GetHttpCode() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return http_code_;
}

int64_t DownloadTaskImpl::GetTotalBytes() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return total_bytes_;
}

int64_t DownloadTaskImpl::GetReceivedBytes() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return received_bytes_;
}

int DownloadTaskImpl::GetPercentComplete() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return percent_complete_;
}

std::string DownloadTaskImpl::GetContentDisposition() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return content_disposition_;
}

std::string DownloadTaskImpl::GetOriginalMimeType() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return original_mime_type_;
}

std::string DownloadTaskImpl::GetMimeType() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return mime_type_;
}

std::u16string DownloadTaskImpl::GetSuggestedFilename() const {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return net::GetSuggestedFilename(GetOriginalUrl(), GetContentDisposition(),
                                   /*referrer_charset=*/std::string(),
                                   /*suggested_name=*/std::string(),
                                   /*mime_type=*/std::string(),
                                   /*default_name=*/"document");
}

bool DownloadTaskImpl::HasPerformedBackgroundDownload() const {
  return has_performed_background_download_;
}

void DownloadTaskImpl::AddObserver(DownloadTaskObserver* observer) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  observers_.AddObserver(observer);
}

void DownloadTaskImpl::RemoveObserver(DownloadTaskObserver* observer) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  observers_.RemoveObserver(observer);
}

void DownloadTaskImpl::OnDownloadUpdated() {
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

void DownloadTaskImpl::OnDownloadFinished(DownloadResult download_result) {
  download_result_ = download_result;
  state_ = State::kComplete;
  OnDownloadUpdated();
}

}  // namespace web
