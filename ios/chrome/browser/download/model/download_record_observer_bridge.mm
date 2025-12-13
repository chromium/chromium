// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_record_observer_bridge.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/download_record.h"

DownloadRecordObserverBridge::DownloadRecordObserverBridge(
    id<DownloadRecordObserverDelegate> delegate)
    : delegate_(delegate) {}

DownloadRecordObserverBridge::~DownloadRecordObserverBridge() = default;

void DownloadRecordObserverBridge::OnDownloadAdded(
    const DownloadRecord& record) {
  [delegate_ downloadRecordWasAdded:record];
}

void DownloadRecordObserverBridge::OnDownloadUpdated(
    const DownloadRecord& record) {
  [delegate_ downloadRecordWasUpdated:record];
}

void DownloadRecordObserverBridge::OnDownloadsRemoved(
    const std::vector<std::string_view>& download_ids) {
  NSMutableArray<NSString*>* ns_download_ids =
      [NSMutableArray arrayWithCapacity:download_ids.size()];

  for (const auto& download_id : download_ids) {
    NSString* ns_download_id =
        base::SysUTF8ToNSString(std::string(download_id));
    [ns_download_ids addObject:ns_download_id];
  }

  [delegate_ downloadsWereRemovedWithIDs:ns_download_ids];
}
