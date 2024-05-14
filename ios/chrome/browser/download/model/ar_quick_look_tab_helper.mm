// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper.h"

#import <memory>
#import <string>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/escape.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper_delegate.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "net/base/url_util.h"

const char kIOSDownloadARModelStateHistogram[] =
    "Download.IOSDownloadARModelState";

const char kUsdzMimeTypeHistogramSuffix[] = ".USDZ";

namespace {

// When an AR Quick Look URL contains this fragment, scaling the displayed
// image (e.g., by pinch-zooming) is disallowed. See
// https://developer.apple.com/videos/play/wwdc2019/612/
const char kContentScalingKey[] = "allowsContentScaling";

// When an AR Quick Look URL contains this fragment, this URL will be used
// when users invoke the share sheet. See
// https://developer.apple.com/videos/play/wwdc2019/612/
const char kCanonicalWebPageURLKey[] = "canonicalWebPageURL";

// Returns a suffix for Download.IOSDownloadARModelState histogram for the
// `download_task`.
std::string GetMimeTypeSuffix(web::DownloadTask* download_task) {
  DCHECK(IsUsdzFileFormat(download_task->GetOriginalMimeType(),
                          download_task->GenerateFileName()));
  return kUsdzMimeTypeHistogramSuffix;
}

// Returns whether the `download_task` is complete or failed.
bool IsDownloadCompleteOrFailed(web::DownloadTask* download_task) {
  switch (download_task->GetState()) {
    case web::DownloadTask::State::kComplete:
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable:
      return YES;
    default:
      return NO;
  }
}

// Returns an enum for Download.IOSDownloadARModelState histogram for the
// terminated `download_task`.
IOSDownloadARModelState GetHistogramEnum(web::DownloadTask* download_task) {
  DCHECK(download_task);
  if (download_task->GetState() == web::DownloadTask::State::kNotStarted) {
    return IOSDownloadARModelState::kCreated;
  }
  if (download_task->GetState() == web::DownloadTask::State::kInProgress) {
    return IOSDownloadARModelState::kStarted;
  }
  DCHECK(download_task->IsDone());
  if (!IsUsdzFileFormat(download_task->GetMimeType(),
                        download_task->GenerateFileName())) {
    return IOSDownloadARModelState::kWrongMimeTypeFailure;
  }
  if (download_task->GetHttpCode() == 401 ||
      download_task->GetHttpCode() == 403) {
    return IOSDownloadARModelState::kUnauthorizedFailure;
  }
  if (download_task->GetErrorCode()) {
    return IOSDownloadARModelState::kOtherFailure;
  }
  return IOSDownloadARModelState::kSuccessful;
}

// Logs Download.IOSDownloadARModelState* histogram for the `download_task`.
void LogHistogram(web::DownloadTask* download_task) {
  DCHECK(download_task);
  base::UmaHistogramEnumeration(
      kIOSDownloadARModelStateHistogram + GetMimeTypeSuffix(download_task),
      GetHistogramEnum(download_task));
}

// Converts the ref of `url` into a query to allow parsing it using
// net::GetValueForKeyInQuery (as net does not provide utilities to
// parse ref).
GURL ConvertRefToQueryInUrl(const GURL& url) {
  GURL::Replacements replacement;
  replacement.SetQueryStr(url.ref_piece());
  replacement.ClearRef();

  return url.ReplaceComponents(replacement);
}

}  // namespace

ARQuickLookTabHelper::ARQuickLookTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
}

ARQuickLookTabHelper::~ARQuickLookTabHelper() {
  if (download_task_) {
    RemoveCurrentDownload();
  }
}

void ARQuickLookTabHelper::Download(
    std::unique_ptr<web::DownloadTask> download_task) {
  DCHECK(download_task);
  if (download_task_) {
    RemoveCurrentDownload();
  }

  base::FilePath download_dir;
  if (!GetTempDownloadsDirectory(&download_dir)) {
    return;
  }

  // Take ownership of `download_task` and start the download.
  download_task_ = std::move(download_task);
  LogHistogram(download_task_.get());
  download_task_->AddObserver(this);

  download_task_->Start(
      download_dir.Append(download_task_->GenerateFileName()));

  // Calling DownloadTask::Start() may cause the task to be immediately
  // destroyed (e.g. if it is in error). Only call `LogHistogram` is it
  // is still valid and owned by the current object.
  if (download_task_)
    LogHistogram(download_task_.get());
}

void ARQuickLookTabHelper::DidFinishDownload() {
  DCHECK(IsDownloadCompleteOrFailed(download_task_.get()));
  // Inform the delegate only if the download has been successful.
  if (download_task_->GetHttpCode() == 401 ||
      download_task_->GetHttpCode() == 403 || download_task_->GetErrorCode() ||
      !IsUsdzFileFormat(download_task_->GetMimeType(),
                        download_task_->GenerateFileName())) {
    return;
  }

  GURL url = download_task_->GetOriginalUrl();
  if (url.SchemeIsBlob()) {
    // If the download was a blob: URL, the task URL looks like the following
    // "blob:https://example.com/...%23..." (i.e. the real URL but some of the
    // characters such as `#` have been encoded). Extract the path from the
    // blob: URL and decode it as a component to recreate the real URL.
    //
    // This is a hack as the https://www.w3.org/TR/FileAPI/#url seems to imply
    // that the fragment needs to be stripped from the URL when creating a blob
    // URL, but this appears to work well enough for https://crbug.com/1341660
    // issue.
    url = GURL(base::UnescapeURLComponent(
        url.path(),
        base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS));
  }

  // Convert the URL ref into query parameter to allow parsing of the URL
  // ref using net::GetValueForKeyInQuery(...) (net doesn't provide a way
  // to parse the ref).
  url = ConvertRefToQueryInUrl(url);

  bool allow_content_scaling = true;
  {
    std::string key_value;
    if (net::GetValueForKeyInQuery(url, kContentScalingKey, &key_value)) {
      // Scaling is disabled if the value is set to 0.
      allow_content_scaling = (key_value != "0");
    }
  }

  NSURL* canonical_url = nil;
  {
    std::string key_value;
    if (net::GetValueForKeyInQuery(url, kCanonicalWebPageURLKey, &key_value)) {
      // Ignore extracted value if not a valid URL.
      const GURL extracted_url(key_value);
      if (extracted_url.is_valid()) {
        canonical_url = net::NSURLWithGURL(extracted_url);
      }
    }
  }

  NSURL* file_url =
      base::apple::FilePathToNSURL(download_task_->GetResponsePath());
  [delegate_ presentUSDZFileWithURL:file_url
                       canonicalURL:canonical_url
                           webState:web_state_
                allowContentScaling:allow_content_scaling];
}

void ARQuickLookTabHelper::RemoveCurrentDownload() {
  download_task_->RemoveObserver(this);
  download_task_.reset();
}

void ARQuickLookTabHelper::OnDownloadUpdated(web::DownloadTask* download_task) {
  DCHECK_EQ(download_task, download_task_.get());

  switch (download_task_->GetState()) {
    case web::DownloadTask::State::kCancelled:
      LogHistogram(download_task_.get());
      RemoveCurrentDownload();
      break;
    case web::DownloadTask::State::kInProgress:
      // Do nothing. Histogram is already logged after the task was started.
      break;
    case web::DownloadTask::State::kComplete:
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable:
      LogHistogram(download_task_.get());
      DidFinishDownload();
      break;
    case web::DownloadTask::State::kNotStarted:
      NOTREACHED_IN_MIGRATION() << "Invalid state.";
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(ARQuickLookTabHelper)
