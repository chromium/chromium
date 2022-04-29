// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/browser_download_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/download/ar_quick_look_tab_helper.h"
#include "ios/chrome/browser/download/download_manager_metric_names.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#include "ios/chrome/browser/download/mime_type_util.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/vcard_tab_helper.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/ui/download/features.h"
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

  if (mime_type == kZipArchiveMimeType)
    return DownloadMimeTypeResult::ZipArchive;

  if (mime_type == kMobileConfigurationType)
    return DownloadMimeTypeResult::iOSMobileConfig;

  if (mime_type == kMicrosoftApplicationMimeType)
    return DownloadMimeTypeResult::MicrosoftApplication;

  if (mime_type == kAndroidPackageArchiveMimeType)
    return DownloadMimeTypeResult::AndroidPackageArchive;

  if (mime_type == kVcardMimeType)
    return DownloadMimeTypeResult::VirtualContactFile;

  if (mime_type == kCalendarMimeType)
    return DownloadMimeTypeResult::Calendar;

  if (mime_type == kLegacyUsdzMimeType)
    return DownloadMimeTypeResult::LegacyUniversalSceneDescription;

  if (mime_type == kAppleDiskImageMimeType)
    return DownloadMimeTypeResult::AppleDiskImage;

  if (mime_type == kAppleInstallerPackageMimeType)
    return DownloadMimeTypeResult::AppleInstallerPackage;

  if (mime_type == kSevenZipArchiveMimeType)
    return DownloadMimeTypeResult::SevenZipArchive;

  if (mime_type == kRARArchiveMimeType)
    return DownloadMimeTypeResult::RARArchive;

  if (mime_type == kTarArchiveMimeType)
    return DownloadMimeTypeResult::TarArchive;

  if (mime_type == kAdobeFlashMimeType)
    return DownloadMimeTypeResult::AdobeFlash;

  if (mime_type == kAmazonKindleBookMimeType)
    return DownloadMimeTypeResult::AmazonKindleBook;

  if (mime_type == kBinaryDataMimeType)
    return DownloadMimeTypeResult::BinaryData;

  if (mime_type == kBitTorrentMimeType)
    return DownloadMimeTypeResult::BitTorrent;

  if (mime_type == kJavaArchiveMimeType)
    return DownloadMimeTypeResult::JavaArchive;

  if (mime_type == kVcardMimeType)
    return DownloadMimeTypeResult::Vcard;

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
  // When a prerendered page tries to download a file, cancel the download.
  PrerenderService* prerender_service =
      PrerenderServiceFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state->GetBrowserState()));
  if (prerender_service &&
      prerender_service->IsWebStatePrerendered(web_state)) {
    return;
  }

  base::UmaHistogramEnumeration("Download.IOSDownloadMimeType",
                                GetUmaResult(task->GetMimeType()));
  base::UmaHistogramEnumeration("Download.IOSDownloadFileUI",
                                DownloadFileUI::DownloadFilePresented,
                                DownloadFileUI::Count);

  if (task->GetMimeType() == kPkPassMimeType) {
    PassKitTabHelper* tab_helper = PassKitTabHelper::FromWebState(web_state);
    if (tab_helper)
      tab_helper->Download(std::move(task));
  } else if (IsUsdzFileFormat(task->GetMimeType(), task->GenerateFileName()) &&
             !base::FeatureList::IsEnabled(kARKillSwitch)) {
    ARQuickLookTabHelper* tab_helper =
        ARQuickLookTabHelper::FromWebState(web_state);
    if (tab_helper)
      tab_helper->Download(std::move(task));

  } else if (task->GetMimeType() == kMobileConfigurationType &&
             task->GetOriginalUrl().SchemeIsHTTPOrHTTPS()) {
    // SFSafariViewController can only open http and https URLs.
    SafariDownloadTabHelper* tab_helper =
        SafariDownloadTabHelper::FromWebState(web_state);
    if (tab_helper)
      tab_helper->DownloadMobileConfig(std::move(task));
  } else if (task->GetMimeType() == kCalendarMimeType &&
             base::FeatureList::IsEnabled(kDownloadCalendar) &&
             task->GetOriginalUrl().SchemeIsHTTPOrHTTPS()) {
    // SFSafariViewController can only open http and https URLs.
    SafariDownloadTabHelper* tab_helper =
        SafariDownloadTabHelper::FromWebState(web_state);
    if (tab_helper)
      tab_helper->DownloadCalendar(std::move(task));
  } else if (task->GetMimeType() == kVcardMimeType &&
             !base::FeatureList::IsEnabled(kVCardKillSwitch)) {
    VcardTabHelper* tab_helper = VcardTabHelper::FromWebState(web_state);
    if (tab_helper)
      tab_helper->Download(std::move(task));
  } else {
    DownloadManagerTabHelper* tab_helper =
        DownloadManagerTabHelper::FromWebState(web_state);
    // TODO(crbug.com/1300151): Investigate why tab_helper is sometimes nil.
    if (tab_helper)
      tab_helper->Download(std::move(task));
  }
}

void BrowserDownloadService::OnDownloadControllerDestroyed(
    web::DownloadController* download_controller) {
  DCHECK_EQ(this, download_controller->GetDelegate());
  download_controller->SetDelegate(nullptr);
  download_controller_ = nullptr;
}
