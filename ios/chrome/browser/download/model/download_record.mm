// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record.h"

#import "base/files/file_path.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "url/gurl.h"

DownloadRecord::DownloadRecord() = default;

DownloadRecord::DownloadRecord(web::DownloadTask* task) {
  DCHECK(task);

  download_id = base::SysNSStringToUTF8(task->GetIdentifier());
  url = task->GetOriginalUrl().spec();
  file_name = task->GenerateFileName().value();
  mime_type = task->GetMimeType();
  created_time = base::Time::Now();
  file_size = task->GetTotalBytes();
  received_bytes = task->GetReceivedBytes();
  total_bytes = task->GetTotalBytes();
  progress_percent = task->GetPercentComplete();
  state = task->GetState();
}

DownloadRecord::DownloadRecord(const DownloadRecord& other) = default;
DownloadRecord& DownloadRecord::operator=(const DownloadRecord& other) =
    default;
DownloadRecord::DownloadRecord(DownloadRecord&& other) = default;
DownloadRecord& DownloadRecord::operator=(DownloadRecord&& other) = default;
DownloadRecord::~DownloadRecord() = default;
