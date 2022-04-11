// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_BROWSER_DOWNLOAD_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_BROWSER_DOWNLOAD_SERVICE_H_

#include <memory>

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
  Calendar = 7,
  // model/usd MIME type.
  LegacyUniversalSceneDescription = 8,
  // application/x-apple-diskimage MIME type.
  AppleDiskImage = 9,
  // application/vnd.apple.installer+xml MIME type.
  AppleInstallerPackage = 10,
  // application/x-7z-compressed MIME type.
  SevenZipArchive = 11,
  // application/x-rar-compressed MIME type.
  RARArchive = 12,
  // application/x-tar MIME type.
  TarArchive = 13,
  // application/x-shockwave-flash MIME type.
  AdobeFlash = 14,
  // application/vnd.amazon.ebook MIME type.
  AmazonKindleBook = 15,
  // application/octet-stream MIME type.
  BinaryData = 16,
  // application/x-bittorrent MIME type.
  BitTorrent = 17,
  // application/java-archive MIME type.
  JavaArchive = 18,
  // model/vnd.pixar.usd MIME type.
  LegacyPixarUniversalSceneDescription = 19,
  // model/vnd.usdz+zip MIME type.
  UniversalSceneDescription = 20,
  // text/vcard MIME type.
  Vcard = 21,
  kMaxValue = Vcard,
};

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

  web::DownloadController* download_controller_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_BROWSER_DOWNLOAD_SERVICE_H_
