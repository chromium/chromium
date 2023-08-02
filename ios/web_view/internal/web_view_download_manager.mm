// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_download_manager.h"

#include "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web_view/internal/cwv_download_task_internal.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "ios/web_view/public/cwv_navigation_delegate.h"

namespace ios_web_view {

WebViewDownloadManager::WebViewDownloadManager(web::BrowserState* browser_state)
    : browser_state_(browser_state),
      download_controller_(
          web::DownloadController::FromBrowserState(browser_state_)) {
  download_controller_->SetDelegate(this);
}

WebViewDownloadManager::~WebViewDownloadManager() {
  if (download_controller_) {
    download_controller_->SetDelegate(nullptr);
  }
}

void WebViewDownloadManager::OnDownloadCreated(
    web::DownloadController*,
    web::WebState* web_state,
    std::unique_ptr<web::DownloadTask> task) {
  CWVWebView* web_view = [CWVWebView webViewForWebState:web_state];
  if ([web_view.navigationDelegate
          respondsToSelector:@selector(webView:didRequestDownloadWithTask:)]) {
    CWVDownloadTask* cwv_task =
        [[CWVDownloadTask alloc] initWithInternalTask:std::move(task)];
    [web_view.navigationDelegate webView:web_view
              didRequestDownloadWithTask:cwv_task];
  }
}

void WebViewDownloadManager::OnDownloadControllerDestroyed(
    web::DownloadController*) {
  download_controller_->SetDelegate(nullptr);
  download_controller_ = nullptr;
}

}  // namespace ios_web_view
