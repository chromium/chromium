// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_BROWSER_DOWNLOAD_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_BROWSER_DOWNLOAD_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/web/public/download/download_controller_delegate.h"

namespace web {
class DownloadController;
class DownloadTask;
class WebState;
}  // namespace web

// Enum for the Download.IOSDownloadMimeType UMA histogram to report the
// MIME type of the download task.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
enum class DownloadMimeTypeResult {
  // MIME type other than those listed below.
  Other = 0,
  // application/vnd.apple.pkpass MIME type.
  PkPass = 1,
  // application/x-apple-aspen-config MIME type.
  iOSMobileConfig = 2,
  // application/zip MIME type.
  ZipArchive = 3,
  // application/x-msdownload MIME type (.exe file).
  MicrosoftApplication = 4,
  // application/vnd.android.package-archive MIME type (.apk file).
  AndroidPackageArchive = 5,
  // text/vcard MIME type.
  VirtualContactFile = 6,
  // text/calendar MIME type.
  iCalendar = 7,
  // model/usd MIME type.
  UniversalSceneDescription = 8,
  kMaxValue = UniversalSceneDescription,
};

// Keyed Service which acts as web::DownloadController delegate and routes
// download tasks to the appropriate TabHelper for download.
class BrowserDownloadService : public KeyedService,
                               public web::DownloadControllerDelegate {
 public:
  explicit BrowserDownloadService(web::DownloadController* download_controller);
  ~BrowserDownloadService() override;

 private:
  // web::DownloadControllerDelegate overrides:
  void OnDownloadCreated(web::DownloadController*,
                         web::WebState*,
                         std::unique_ptr<web::DownloadTask>) override;
  void OnDownloadControllerDestroyed(web::DownloadController*) override;

  web::DownloadController* download_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BrowserDownloadService);
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_BROWSER_DOWNLOAD_SERVICE_H_
