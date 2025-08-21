// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import <string>
#import <string_view>

#import "ios/chrome/browser/download/model/download_record_observer.h"
#import "ios/web/public/download/download_task.h"

struct DownloadRecord;

// Protocol for receiving download record observer callbacks.
@protocol DownloadRecordObserverDelegate <NSObject>

// Called when a download record is added.
- (void)downloadRecordWasAdded:(const DownloadRecord&)record;

// Called when a download record is updated.
- (void)downloadRecordWasUpdatedWithID:(NSString*)downloadID
                                 state:(int)newState;

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
  void OnDownloadUpdated(std::string_view download_id,
                         web::DownloadTask::State new_state) override;

 private:
  __weak id<DownloadRecordObserverDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_OBSERVER_BRIDGE_H_
