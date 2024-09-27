// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/browser_download_service.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/model/download_manager_metric_names.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_mimetype_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/download/ui_bundled/features.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/url_util.h"

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
  PrerenderService* prerender_service = PrerenderServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
  if (prerender_service &&
      prerender_service->IsWebStatePrerendered(web_state)) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Download.IOSDownloadMimeType",
      GetDownloadMimeTypeResultFromMimeType(task->GetMimeType()));
  base::UmaHistogramEnumeration("Download.IOSDownloadFileUI",
                                DownloadFileUI::DownloadFilePresented,
                                DownloadFileUI::Count);

  if ((task->GetMimeType() == kPkPassMimeType ||
       task->GetMimeType() == kPkBundledPassMimeType) &&
      !base::FeatureList::IsEnabled(kPassKitKillSwitch)) {
    PassKitTabHelper* tab_helper =
        PassKitTabHelper::GetOrCreateForWebState(web_state);
    if (tab_helper)
      tab_helper->Download(std::move(task));
  } else if (IsUsdzFileFormat(task->GetMimeType(), task->GenerateFileName()) &&
             !base::FeatureList::IsEnabled(kARKillSwitch)) {
    ARQuickLookTabHelper* tab_helper =
        ARQuickLookTabHelper::GetOrCreateForWebState(web_state);
    if (tab_helper)
      tab_helper->Download(std::move(task));

  } else if (task->GetMimeType() == kMobileConfigurationType &&
             (task->GetOriginalUrl().SchemeIsCryptographic() ||
              net::IsLocalhost(task->GetOriginalUrl()))) {
    // SFSafariViewController can only open http and https URLs.
    SafariDownloadTabHelper* tab_helper =
        SafariDownloadTabHelper::FromWebState(web_state);
    if (tab_helper)
      tab_helper->DownloadMobileConfig(std::move(task));
  } else if (task->GetMimeType() == kCalendarMimeType &&
             !base::FeatureList::IsEnabled(kCalendarKillSwitch) &&
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
    // TODO(crbug.com/40216128): Investigate why tab_helper is sometimes nil.
    if (tab_helper)
      tab_helper->SetCurrentDownload(std::move(task));
  }
}

void BrowserDownloadService::OnDownloadControllerDestroyed(
    web::DownloadController* download_controller) {
  DCHECK_EQ(this, download_controller->GetDelegate());
  download_controller->SetDelegate(nullptr);
  download_controller_ = nullptr;
}
