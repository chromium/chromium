// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_DOWNLOAD_MANAGER_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_DOWNLOAD_MANAGER_H_

#include <memory>

#include "ios/web/public/download/download_controller_delegate.h"

namespace web {
class BrowserState;
class DownloadTask;
class WebState;
}  // namespace web

namespace ios_web_view {

// A class to handle browser downloads in //ios/web_view.
class WebViewDownloadManager : public web::DownloadControllerDelegate {
 public:
  explicit WebViewDownloadManager(web::BrowserState* browser_state);
  ~WebViewDownloadManager() override;
  void OnDownloadCreated(web::DownloadController* download_controller,
                         web::WebState* web_state,
                         std::unique_ptr<web::DownloadTask> task) override;
  void OnDownloadControllerDestroyed(
      web::DownloadController* download_controller) override;

 private:
  web::BrowserState* browser_state_ = nullptr;
  web::DownloadController* download_controller_ = nullptr;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_DOWNLOAD_MANAGER_H_
