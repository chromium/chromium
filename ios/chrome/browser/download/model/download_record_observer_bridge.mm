// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_observer_bridge.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/web/public/download/download_task.h"

DownloadRecordObserverBridge::DownloadRecordObserverBridge(
    id<DownloadRecordObserverDelegate> delegate)
    : delegate_(delegate) {}

DownloadRecordObserverBridge::~DownloadRecordObserverBridge() = default;

void DownloadRecordObserverBridge::OnDownloadAdded(
    const DownloadRecord& record) {
  [delegate_ downloadRecordWasAdded:record];
}

void DownloadRecordObserverBridge::OnDownloadUpdated(
    std::string_view download_id,
    web::DownloadTask::State new_state) {
  NSString* ns_download_id = base::SysUTF8ToNSString(std::string(download_id));
  [delegate_ downloadRecordWasUpdatedWithID:ns_download_id
                                      state:static_cast<int>(new_state)];
}
