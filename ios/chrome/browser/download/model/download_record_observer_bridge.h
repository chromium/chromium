// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import <string>
#import <string_view>

#import "ios/chrome/browser/download/model/download_record_observer.h"

struct DownloadRecord;

// Protocol for receiving download record observer callbacks.
@protocol DownloadRecordObserverDelegate <NSObject>

// Called when a download record is added.
- (void)downloadRecordWasAdded:(const DownloadRecord&)record;

// Called when a download record is updated.
- (void)downloadRecordWasUpdated:(const DownloadRecord&)record;

// Called when download records are removed.
- (void)downloadsWereRemovedWithIDs:(NSArray<NSString*>*)downloadIDs;

@end

// C++ observer bridge that implements DownloadRecordObserver and forwards
// callbacks to the Objective-C delegate.
class DownloadRecordObserverBridge : public DownloadRecordObserver {
 public:
  explicit DownloadRecordObserverBridge(
      id<DownloadRecordObserverDelegate> delegate);

  DownloadRecordObserverBridge(const DownloadRecordObserverBridge&) = delete;
  DownloadRecordObserverBridge& operator=(const DownloadRecordObserverBridge&) =
      delete;

  ~DownloadRecordObserverBridge() override;

  // DownloadRecordObserver implementation.
  void OnDownloadAdded(const DownloadRecord& record) override;
  void OnDownloadUpdated(const DownloadRecord& record) override;
  void OnDownloadsRemoved(
      const std::vector<std::string_view>& download_ids) override;

 private:
  __weak id<DownloadRecordObserverDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_BRIDGE_H_
