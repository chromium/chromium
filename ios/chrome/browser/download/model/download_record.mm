// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record.h"

#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

DownloadRecord::DownloadRecord() = default;

DownloadRecord::DownloadRecord(web::DownloadTask* task) {
  DCHECK(task);

  download_id = base::SysNSStringToUTF8(task->GetIdentifier());
  original_url = task->GetOriginalUrl().spec();
  redirected_url = task->GetRedirectedUrl().spec();
  file_name = task->GenerateFileName().value();
  // Only get response path if download is completed to avoid DCHECK crash.
  if (task->IsDone()) {
    response_path = task->GetResponsePath();
  }
  original_mime_type = task->GetOriginalMimeType();
  mime_type = task->GetMimeType();
  content_disposition = task->GetContentDisposition();
  originating_host = base::SysNSStringToUTF8(task->GetOriginatingHost());
  http_method = base::SysNSStringToUTF8(task->GetHttpMethod());
  http_code = task->GetHttpCode();
  error_code = task->GetErrorCode();
  received_bytes = task->GetReceivedBytes();
  total_bytes = task->GetTotalBytes();
  progress_percent = task->GetPercentComplete();
  state = task->GetState();
  has_performed_background_download = task->HasPerformedBackgroundDownload();
}

DownloadRecord::DownloadRecord(const DownloadRecord& other) = default;
DownloadRecord& DownloadRecord::operator=(const DownloadRecord& other) =
    default;
DownloadRecord::DownloadRecord(DownloadRecord&& other) = default;
DownloadRecord& DownloadRecord::operator=(DownloadRecord&& other) = default;
DownloadRecord::~DownloadRecord() = default;
