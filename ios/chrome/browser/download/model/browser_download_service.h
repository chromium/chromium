// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_H_

#include <memory>

#import "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/web/public/download/download_controller_delegate.h"

namespace web {
class DownloadController;
class DownloadTask;
class WebState;
}  // namespace web

// Keyed Service which acts as web::DownloadController delegate and routes
// download tasks to the appropriate TabHelper for download.
class BrowserDownloadService : public KeyedService,
                               public web::DownloadControllerDelegate {
 public:
  explicit BrowserDownloadService(web::DownloadController* download_controller);

  BrowserDownloadService(const BrowserDownloadService&) = delete;
  BrowserDownloadService& operator=(const BrowserDownloadService&) = delete;

  ~BrowserDownloadService() override;

 private:
  // web::DownloadControllerDelegate overrides:
  void OnDownloadCreated(web::DownloadController*,
                         web::WebState*,
                         std::unique_ptr<web::DownloadTask>) override;
  void OnDownloadControllerDestroyed(web::DownloadController*) override;

  raw_ptr<web::DownloadController> download_controller_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_H_
