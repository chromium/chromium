// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/browser_download_service.h"

#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#include "ios/chrome/browser/download/pass_kit_mime_type.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#include "ios/chrome/browser/download/usdz_mime_type.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns DownloadMimeTypeResult for the given MIME type.
DownloadMimeTypeResult GetUmaResult(const std::string& mime_type) {
  if (mime_type == kPkPassMimeType)
    return DownloadMimeTypeResult::PkPass;

  if (mime_type == "application/zip")
    return DownloadMimeTypeResult::ZipArchive;

  if (mime_type == "application/x-apple-aspen-config")
    return DownloadMimeTypeResult::iOSMobileConfig;

  if (mime_type == "application/x-msdownload")
    return DownloadMimeTypeResult::MicrosoftApplication;

  if (mime_type == "application/vnd.android.package-archive")
    return DownloadMimeTypeResult::AndroidPackageArchive;

  if (mime_type == "text/vcard")
    return DownloadMimeTypeResult::VirtualContactFile;

  if (mime_type == "text/calendar")
    return DownloadMimeTypeResult::iCalendar;

  if (mime_type == kLegacyUsdzMimeType)
    return DownloadMimeTypeResult::LegacyUniversalSceneDescription;

  if (mime_type == "application/x-apple-diskimage")
    return DownloadMimeTypeResult::AppleDiskImage;

  if (mime_type == "application/vnd.apple.installer+xml")
    return DownloadMimeTypeResult::AppleInstallerPackage;

  if (mime_type == "application/x-7z-compressed")
    return DownloadMimeTypeResult::SevenZipArchive;

  if (mime_type == "application/x-rar-compressed")
    return DownloadMimeTypeResult::RARArchive;

  if (mime_type == "application/x-tar")
    return DownloadMimeTypeResult::TarArchive;

  if (mime_type == "application/x-shockwave-flash")
    return DownloadMimeTypeResult::AdobeFlash;

  if (mime_type == "application/vnd.amazon.ebook")
    return DownloadMimeTypeResult::AmazonKindleBook;

  if (mime_type == "application/octet-stream")
    return DownloadMimeTypeResult::BinaryData;

  if (mime_type == "application/x-bittorrent")
    return DownloadMimeTypeResult::BitTorrent;

  if (mime_type == "application/java-archive")
    return DownloadMimeTypeResult::JavaArchive;

  if (mime_type == kLegacyPixarUsdzMimeType)
    return DownloadMimeTypeResult::LegacyPixarUniversalSceneDescription;

  if (mime_type == kUsdzMimeType)
    return DownloadMimeTypeResult::UniversalSceneDescription;

  return DownloadMimeTypeResult::Other;
}
}  // namespace

BrowserDownloadService::BrowserDownloadService(
    web::DownloadController* download_controller)
    : download_controller_(download_controller) {
  DCHECK(!download_controller->GetDelegate());
  download_controller_->SetDelegate(this);
}

BrowserDownloadService::~BrowserDownloadService() {
  if (download_controller_) {
    DCHECK_EQ(this, download_controller_->GetDelegate());
    download_controller_->SetDelegate(nullptr);
  }
}

void BrowserDownloadService::OnDownloadCreated(
    web::DownloadController* download_controller,
    web::WebState* web_state,
    std::unique_ptr<web::DownloadTask> task) {
  UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadMimeType",
                            GetUmaResult(task->GetMimeType()));

  if (task->GetMimeType() == kPkPassMimeType) {
    PassKitTabHelper* tab_helper = PassKitTabHelper::FromWebState(web_state);
    if (tab_helper) {
      tab_helper->Download(std::move(task));
    }
  } else if (IsUsdzFileFormat(task->GetMimeType(),
                              task->GetSuggestedFilename())) {
    ARQuickLookTabHelper* tab_helper =
        ARQuickLookTabHelper::FromWebState(web_state);
    if (tab_helper) {
      tab_helper->Download(std::move(task));
    }
  } else {
    DownloadManagerTabHelper* tab_helper =
        DownloadManagerTabHelper::FromWebState(web_state);
    if (tab_helper) {
      tab_helper->Download(std::move(task));
    }
  }
}

void BrowserDownloadService::OnDownloadControllerDestroyed(
    web::DownloadController* download_controller) {
  DCHECK_EQ(this, download_controller->GetDelegate());
  download_controller->SetDelegate(nullptr);
  download_controller_ = nullptr;
}
