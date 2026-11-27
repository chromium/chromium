// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"

#import "base/check.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"

#pragma mark - Initialization

SafariDownloadTabHelper::SafariDownloadTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_);
}

SafariDownloadTabHelper::~SafariDownloadTabHelper() = default;

#pragma mark - Public

void SafariDownloadTabHelper::DownloadMobileConfig(
    std::unique_ptr<web::DownloadTask> task) {
  if (!web_state_->IsVisible()) {
    return;
  }
  NSURL* url = net::NSURLWithGURL(task->GetOriginalUrl());
  [delegate_ presentMobileConfigAlertFromURL:url];
}

void SafariDownloadTabHelper::DownloadCalendar(
    std::unique_ptr<web::DownloadTask> task) {
  if (!web_state_->IsVisible()) {
    return;
  }
  NSURL* url = net::NSURLWithGURL(task->GetOriginalUrl());
  [delegate_ presentCalendarAlertFromURL:url];
}

void SafariDownloadTabHelper::DownloadAppleWalletOrder(
    std::unique_ptr<web::DownloadTask> task) {
  if (!web_state_->IsVisible()) {
    return;
  }
  NSURL* url = net::NSURLWithGURL(task->GetOriginalUrl());
  [delegate_ presentAppleWalletOrderAlertFromURL:url];
}
