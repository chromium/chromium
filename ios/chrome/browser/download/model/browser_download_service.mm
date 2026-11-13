// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/browser_download_service.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"
#import "ios/chrome/browser/download/model/download_manager_metric_names.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_mimetype_util.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/model/vcard_tab_helper.h"
#import "ios/chrome/browser/download/ui/features.h"
#import "ios/chrome/browser/drive/model/drive_availability.h"
#import "ios/chrome/browser/drive/model/drive_policy.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/prerender/model/prerender_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/url_util.h"

namespace {

// Categories representing the specialized tab helpers (or standard manager)
// responsible for handling specific download tasks based on MIME type, file
// format, feature flags, and URL scheme.
enum class DownloadRoutingCategory {
  kPassKit,
  kARQuickLook,
  kMobileConfig,
  kCalendar,
  kAppleWalletOrder,
  kVCard,
  kStandard,
};

// Evaluates the incoming download task's MIME type, filename, active feature
// kill switches, and cryptographic schemes to determine which specialized tab
// helper or standard download manager should handle the download.
DownloadRoutingCategory GetDownloadRoutingCategory(
    const web::DownloadTask* task) {
  const std::string& mime_type = task->GetMimeType();
  if ((mime_type == kPkPassMimeType || mime_type == kPkBundledPassMimeType) &&
      !base::FeatureList::IsEnabled(kPassKitKillSwitch)) {
    return DownloadRoutingCategory::kPassKit;
  }
  if (IsUsdzFileFormat(mime_type, task->GenerateFileName()) &&
      !base::FeatureList::IsEnabled(kARKillSwitch)) {
    return DownloadRoutingCategory::kARQuickLook;
  }
  if (mime_type == kMobileConfigurationType &&
      (task->GetOriginalUrl().SchemeIsCryptographic() ||
       net::IsLocalhost(task->GetOriginalUrl()))) {
    return DownloadRoutingCategory::kMobileConfig;
  }
  if (mime_type == kCalendarMimeType &&
      !base::FeatureList::IsEnabled(kCalendarKillSwitch) &&
      task->GetOriginalUrl().SchemeIsHTTPOrHTTPS()) {
    return DownloadRoutingCategory::kCalendar;
  }
  if (mime_type == kAppleWalletOrderMimeType &&
      task->GetOriginalUrl().SchemeIsHTTPOrHTTPS()) {
    return DownloadRoutingCategory::kAppleWalletOrder;
  }
  if ((mime_type == kVcardMimeType || mime_type == kXVcardMimeType) &&
      !base::FeatureList::IsEnabled(kVCardKillSwitch)) {
    return DownloadRoutingCategory::kVCard;
  }
  return DownloadRoutingCategory::kStandard;
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

// static
bool BrowserDownloadService::ShouldRestrictLocalDownloads(
    web::WebState* web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  PrefService* pref_service = profile->GetPrefs();
  return static_cast<policy::DownloadRestriction>(pref_service->GetInteger(
             policy::policy_prefs::kDownloadRestrictions)) ==
         policy::DownloadRestriction::ALL_FILES;
}

// static
bool BrowserDownloadService::ShouldRestrictAllDownloads(
    web::WebState* web_state) {
  if (!ShouldRestrictLocalDownloads(web_state)) {
    return false;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  PrefService* pref_service = profile->GetPrefs();
  drive::DriveService* drive_service =
      drive::DriveServiceFactory::GetForProfile(profile);
  if (!drive_service || !drive_service->IsSupported()) {
    return true;
  }
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!auth_service) {
    return true;
  }
  bool is_save_to_drive_available = drive::IsSaveToDriveAvailable(
      profile->IsOffTheRecord(), drive_service, pref_service, auth_service);
  return !is_save_to_drive_available;
}

void BrowserDownloadService::OnDownloadCreated(
    web::DownloadController* download_controller,
    web::WebState* web_state,
    std::unique_ptr<web::DownloadTask> task) {
  // When a prerendered page tries to download a file, cancel the download.
  if (PrerenderTabHelper::FromWebState(web_state)) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Download.IOSDownloadMimeType",
      GetDownloadMimeTypeResultFromMimeType(task->GetMimeType()));
  base::UmaHistogramEnumeration("Download.IOSDownloadFileUI",
                                DownloadFileUI::DownloadFilePresented,
                                DownloadFileUI::Count);

  DownloadRoutingCategory category = GetDownloadRoutingCategory(task.get());

  // Determine whether this download should be restricted by enterprise policy.
  // The restriction conditions differ depending on the routing category:
  // - Standard downloads (handled by DownloadManagerTabHelper) support saving
  //   to Google Drive. Therefore, they evaluate ShouldRestrictAllDownloads(),
  //   which allows the download if Save to Google Drive is available even when
  //   local downloads are restricted.
  // - Specialized downloads (PassKit, AR Quick Look .usdz, MobileConfig,
  //   Calendar, Apple Wallet Order, VCard) are opened directly by specialized
  //   iOS system apps or view controllers and do NOT support saving to Google
  //   Drive. Therefore, they evaluate ShouldRestrictLocalDownloads(), which
  //   unconditionally restricts the download whenever local downloads are
  //   restricted.
  bool is_restricted = (category == DownloadRoutingCategory::kStandard)
                           ? ShouldRestrictAllDownloads(web_state)
                           : ShouldRestrictLocalDownloads(web_state);

  if (is_restricted) {
    if (web_state && web_state->IsVisible()) {
      if (DownloadManagerTabHelper* tab_helper =
              DownloadManagerTabHelper::FromWebState(web_state)) {
        tab_helper->ShowRestrictDownloadSnackbar();
      }
    }
    return;
  }

  switch (category) {
    case DownloadRoutingCategory::kPassKit: {
      if (PassKitTabHelper* tab_helper =
              PassKitTabHelper::FromWebState(web_state)) {
        tab_helper->Download(std::move(task));
      }
      break;
    }
    case DownloadRoutingCategory::kARQuickLook: {
      if (ARQuickLookTabHelper* tab_helper =
              ARQuickLookTabHelper::FromWebState(web_state)) {
        tab_helper->Download(std::move(task));
      }
      break;
    }
    case DownloadRoutingCategory::kMobileConfig: {
      if (SafariDownloadTabHelper* tab_helper =
              SafariDownloadTabHelper::FromWebState(web_state)) {
        tab_helper->DownloadMobileConfig(std::move(task));
      }
      break;
    }
    case DownloadRoutingCategory::kCalendar: {
      if (SafariDownloadTabHelper* tab_helper =
              SafariDownloadTabHelper::FromWebState(web_state)) {
        tab_helper->DownloadCalendar(std::move(task));
      }
      break;
    }
    case DownloadRoutingCategory::kAppleWalletOrder: {
      if (SafariDownloadTabHelper* tab_helper =
              SafariDownloadTabHelper::FromWebState(web_state)) {
        tab_helper->DownloadAppleWalletOrder(std::move(task));
      }
      break;
    }
    case DownloadRoutingCategory::kVCard: {
      if (VcardTabHelper* tab_helper =
              VcardTabHelper::FromWebState(web_state)) {
        tab_helper->Download(std::move(task));
      }
      break;
    }
    case DownloadRoutingCategory::kStandard: {
      if (DownloadManagerTabHelper* tab_helper =
              DownloadManagerTabHelper::FromWebState(web_state)) {
        tab_helper->SetCurrentDownload(std::move(task));
      }
      break;
    }
  }
}

void BrowserDownloadService::OnDownloadControllerDestroyed(
    web::DownloadController* download_controller) {
  DCHECK_EQ(this, download_controller->GetDelegate());
  download_controller->SetDelegate(nullptr);
  download_controller_ = nullptr;
}
