// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/open_in/open_in_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/filename_util.h"
#import "net/http/http_response_headers.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

void OpenInTabHelper::SetDelegate(id<OpenInTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

OpenInTabHelper::~OpenInTabHelper() {
  // In case that the destructor is called before WebStateDestroyed. stop
  // observing the WebState.
  if (web_state_) {
    [delegate_ destroyOpenInForWebState:web_state_];
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

OpenInMimeType OpenInTabHelper::GetUmaResult(
    const std::string& mime_type) const {
  if (mime_type == content_type::kMimeTypePDF)
    return OpenInMimeType::kMimeTypePDF;
  if (mime_type == content_type::kMimeTypeMicrosoftWord)
    return OpenInMimeType::kMimeTypeMicrosoftWord;
  if (mime_type == content_type::kMimeTypeMicrosoftWordOpenXML)
    return OpenInMimeType::kMimeTypeMicrosoftWordOpenXML;
  if (mime_type == content_type::kMimeTypeJPEG)
    return OpenInMimeType::kMimeTypeJPEG;
  if (mime_type == content_type::kMimeTypePNG)
    return OpenInMimeType::kMimeTypePNG;
  if (mime_type == content_type::kMimeTypeMicrosoftPowerPoint)
    return OpenInMimeType::kMimeTypeMicrosoftPowerPoint;
  if (mime_type == content_type::kMimeTypeMicrosoftPowerPointOpenXML)
    return OpenInMimeType::kMimeTypeMicrosoftPowerPointOpenXML;
  if (mime_type == content_type::kMimeTypeRTF)
    return OpenInMimeType::kMimeTypeRTF;
  if (mime_type == content_type::kMimeTypeSVG)
    return OpenInMimeType::kMimeTypeSVG;
  if (mime_type == content_type::kMimeTypeMicrosoftExcel)
    return OpenInMimeType::kMimeTypeMicrosoftExcel;
  if (mime_type == content_type::kMimeTypeMicrosoftExcelOpenXML)
    return OpenInMimeType::kMimeTypeMicrosoftExcelOpenXML;
  return OpenInMimeType::kMimeTypeNotHandled;
}

void OpenInTabHelper::HandleExportableFile() {
  OpenInMimeType mime_type = GetUmaResult(web_state_->GetContentsMimeType());
  if (mime_type == OpenInMimeType::kMimeTypeNotHandled)
    return;

  DCHECK_NE(mime_type, OpenInMimeType::kMimeTypeNotHandled);
  base::UmaHistogramEnumeration("IOS.OpenIn.MimeType", mime_type);
  base::RecordAction(base::UserMetricsAction("IOS.OpenIn.Presented"));

  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  const GURL& last_committed_url = item ? item->GetURL() : GURL::EmptyGURL();
  std::u16string file_name = GetFileNameSuggestion();
  [delegate_ enableOpenInForWebState:web_state_
                     withDocumentURL:last_committed_url
                   suggestedFileName:base::SysUTF16ToNSString(file_name)];
}

void OpenInTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  [delegate_ disableOpenInForWebState:web_state];
}

void OpenInTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Retrieve the response headers to be used in case the Page loaded
  // successfully (PageLoaded WebStateObserver method will always be called
  // immediatly after DidFinishNavigation).
  response_headers_ = scoped_refptr<net::HttpResponseHeaders>(
      navigation_context->GetResponseHeaders());
}

OpenInTabHelper::OpenInTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

void OpenInTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS)
    HandleExportableFile();
}

void OpenInTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  [delegate_ destroyOpenInForWebState:web_state];
  delegate_ = nil;
  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  // Unregistration as an observer happens in the destructor.
  web_state_->RemoveUserData(UserDataKey());
}

std::u16string OpenInTabHelper::GetFileNameSuggestion() {
  // Try to generate a filename by first looking at `content_disposition`, then
  // at the last component of WebState's last committed URL and if both of these
  // fail use the default filename "document".
  std::string content_disposition;
  if (response_headers_)
    response_headers_->GetNormalizedHeader("content-disposition",
                                           &content_disposition);
  std::string default_file_name =
      l10n_util::GetStringUTF8(IDS_IOS_OPEN_IN_FILE_DEFAULT_TITLE);
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  const GURL& last_committed_url = item ? item->GetURL() : GURL::EmptyGURL();
  std::u16string file_name =
      net::GetSuggestedFilename(last_committed_url, content_disposition,
                                "",  // referrer-charset
                                "",  // suggested-name
                                web_state_->GetContentsMimeType(),  // mime-type
                                default_file_name);
  return file_name;
}

// static
bool OpenInTabHelper::ShouldDownload(web::WebState* web_state) {
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

WEB_STATE_USER_DATA_KEY_IMPL(OpenInTabHelper)
