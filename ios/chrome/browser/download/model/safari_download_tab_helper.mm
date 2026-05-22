// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"

#import "ios/chrome/browser/download/model/safari_download_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/apple/url_conversions.h"

#pragma mark - Initialization

SafariDownloadTabHelper::SafariDownloadTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_);
  web_state_observation_.Observe(web_state);
}

SafariDownloadTabHelper::~SafariDownloadTabHelper() = default;

#pragma mark - Public

void SafariDownloadTabHelper::DownloadMobileConfig(
    std::unique_ptr<web::DownloadTask> task) {
  NSURL* url = net::NSURLWithGURL(task->GetOriginalUrl());
  if (web_state_->IsVisible()) {
    [delegate_ presentMobileConfigAlertFromURL:url];
  } else {
    pending_download_ = {PendingType::kMobileConfig, url};
  }
}

void SafariDownloadTabHelper::DownloadCalendar(
    std::unique_ptr<web::DownloadTask> task) {
  NSURL* url = net::NSURLWithGURL(task->GetOriginalUrl());
  if (web_state_->IsVisible()) {
    [delegate_ presentCalendarAlertFromURL:url];
  } else {
    pending_download_ = {PendingType::kCalendar, url};
  }
}

void SafariDownloadTabHelper::DownloadAppleWalletOrder(
    std::unique_ptr<web::DownloadTask> task) {
  NSURL* url = net::NSURLWithGURL(task->GetOriginalUrl());
  if (web_state_->IsVisible()) {
    [delegate_ presentAppleWalletOrderAlertFromURL:url];
  } else {
    pending_download_ = {PendingType::kAppleWalletOrder, url};
  }
}

#pragma mark - WebStateObserver

void SafariDownloadTabHelper::WasShown(web::WebState* web_state) {
  CHECK_EQ(web_state_, web_state);
  if (delegate_ && pending_download_.has_value()) {
    switch (pending_download_->type) {
      case PendingType::kMobileConfig:
        [delegate_ presentMobileConfigAlertFromURL:pending_download_->url];
        break;
      case PendingType::kCalendar:
        [delegate_ presentCalendarAlertFromURL:pending_download_->url];
        break;
      case PendingType::kAppleWalletOrder:
        [delegate_ presentAppleWalletOrderAlertFromURL:pending_download_->url];
        break;
    }
    pending_download_.reset();
  }
}
