// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/model/share_file_download_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/filename_util.h"
#import "net/http/http_response_headers.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace content_type {

const char kMimeTypeMicrosoftPowerPointOpenXML[] =
    "application/vnd.openxmlformats-officedocument.presentationml.presentation";

const char kMimeTypeMicrosoftWordOpenXML[] =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document";

const char kMimeTypeMicrosoftExcelOpenXML[] =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";

const char kMimeTypePDF[] = "application/pdf";

const char kMimeTypeMicrosoftWord[] = "application/msword";

const char kMimeTypeJPEG[] = "image/jpeg";

const char kMimeTypePNG[] = "image/png";

const char kMimeTypeMicrosoftPowerPoint[] = "application/vnd.ms-powerpoint";

const char kMimeTypeRTF[] = "application/rtf";

const char kMimeTypeSVG[] = "image/svg+xml";

const char kMimeTypeMicrosoftExcel[] = "application/vnd.ms-excel";

}  // namespace content_type

ShareFileDownloadTabHelper::~ShareFileDownloadTabHelper() {
  // In case that the destructor is called before WebStateDestroyed. stop
  // observing the WebState.
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

ShareFileDownloadMimeType ShareFileDownloadTabHelper::GetUmaResult(
    const std::string& mime_type) const {
  if (mime_type == content_type::kMimeTypePDF) {
    return ShareFileDownloadMimeType::kMimeTypePDF;
  }
  if (mime_type == content_type::kMimeTypeMicrosoftWord) {
    return ShareFileDownloadMimeType::kMimeTypeMicrosoftWord;
  }
  if (mime_type == content_type::kMimeTypeMicrosoftWordOpenXML) {
    return ShareFileDownloadMimeType::kMimeTypeMicrosoftWordOpenXML;
  }
  if (mime_type == content_type::kMimeTypeJPEG) {
    return ShareFileDownloadMimeType::kMimeTypeJPEG;
  }
  if (mime_type == content_type::kMimeTypePNG) {
    return ShareFileDownloadMimeType::kMimeTypePNG;
  }
  if (mime_type == content_type::kMimeTypeMicrosoftPowerPoint) {
    return ShareFileDownloadMimeType::kMimeTypeMicrosoftPowerPoint;
  }
  if (mime_type == content_type::kMimeTypeMicrosoftPowerPointOpenXML) {
    return ShareFileDownloadMimeType::kMimeTypeMicrosoftPowerPointOpenXML;
  }
  if (mime_type == content_type::kMimeTypeRTF) {
    return ShareFileDownloadMimeType::kMimeTypeRTF;
  }
  if (mime_type == content_type::kMimeTypeSVG) {
    return ShareFileDownloadMimeType::kMimeTypeSVG;
  }
  if (mime_type == content_type::kMimeTypeMicrosoftExcel) {
    return ShareFileDownloadMimeType::kMimeTypeMicrosoftExcel;
  }
  if (mime_type == content_type::kMimeTypeMicrosoftExcelOpenXML) {
    return ShareFileDownloadMimeType::kMimeTypeMicrosoftExcelOpenXML;
  }
  return ShareFileDownloadMimeType::kMimeTypeNotHandled;
}

void ShareFileDownloadTabHelper::HandleExportableFile() {
  ShareFileDownloadMimeType mime_type =
      GetUmaResult(web_state_->GetContentsMimeType());
  if (mime_type == ShareFileDownloadMimeType::kMimeTypeNotHandled) {
    return;
  }

  CHECK_NE(mime_type, ShareFileDownloadMimeType::kMimeTypeNotHandled);
  base::UmaHistogramEnumeration("IOS.OpenIn.MimeType", mime_type);
}

void ShareFileDownloadTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Retrieve the response headers to be used in case the Page loaded
  // successfully (PageLoaded WebStateObserver method will always be called
  // immediatly after DidFinishNavigation).
  response_headers_ = scoped_refptr<net::HttpResponseHeaders>(
      navigation_context->GetResponseHeaders());
}

ShareFileDownloadTabHelper::ShareFileDownloadTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

void ShareFileDownloadTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    HandleExportableFile();
  }
}

void ShareFileDownloadTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  // Unregistration as an observer happens in the destructor.
  web_state_->RemoveUserData(UserDataKey());
}

std::u16string ShareFileDownloadTabHelper::GetFileNameSuggestion() {
  // Try to generate a filename by first looking at `content_disposition`, then
  // at the last component of WebState's last committed URL and if both of these
  // fail use the default filename "document".
  std::string content_disposition;
  if (response_headers_) {
    response_headers_->GetNormalizedHeader("content-disposition",
                                           &content_disposition);
  }
  std::string default_file_name =
      l10n_util::GetStringUTF8(IDS_IOS_OPEN_IN_FILE_DEFAULT_TITLE);
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  const GURL& last_committed_url = item ? item->GetURL() : GURL();
  std::u16string file_name =
      net::GetSuggestedFilename(last_committed_url, content_disposition,
                                "",  // referrer-charset
                                "",  // suggested-name
                                web_state_->GetContentsMimeType(),  // mime-type
                                default_file_name);
  return file_name;
}

// static
bool ShareFileDownloadTabHelper::ShouldDownload(web::WebState* web_state) {
  if (!web_state) {
    return false;
  }

  std::string mime_type = web_state->GetContentsMimeType();
  return (mime_type == content_type::kMimeTypePDF ||
          mime_type == content_type::kMimeTypeMicrosoftWord ||
          mime_type == content_type::kMimeTypeMicrosoftWordOpenXML ||
          mime_type == content_type::kMimeTypeJPEG ||
          mime_type == content_type::kMimeTypePNG ||
          mime_type == content_type::kMimeTypeMicrosoftPowerPoint ||
          mime_type == content_type::kMimeTypeMicrosoftPowerPointOpenXML ||
          mime_type == content_type::kMimeTypeRTF ||
          mime_type == content_type::kMimeTypeSVG ||
          mime_type == content_type::kMimeTypeMicrosoftExcel ||
          mime_type == content_type::kMimeTypeMicrosoftExcelOpenXML);
}

WEB_STATE_USER_DATA_KEY_IMPL(ShareFileDownloadTabHelper)
