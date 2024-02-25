// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_OBSERVER_BRIDGE_H_
#define IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/download/download_task_observer.h"

// Observes download task lifecycle events from Objective-C. To use as a
// web::DownloadTaskObserver, wrap in a web::DownloadTaskObserverBridge.
@protocol CRWDownloadTaskObserver <NSObject>
@optional

// Invoked by DownloadTaskObserverBridge::OnDownloadUpdated.
- (void)downloadUpdated:(web::DownloadTask*)task;

// Invoked by DownloadTaskObserverBridge::OnDownloadDestroyed.
- (void)downloadDestroyed:(web::DownloadTask*)task;

@end

namespace web {

// Bridge to use an id<CRWDownloadTaskObserver> as a web::DownloadTaskObserver.
class DownloadTaskObserverBridge : public web::DownloadTaskObserver {
 public:
  // It is the responsibility of calling code to add/remove the instance
  // from the DownloadTask observer lists.
  explicit DownloadTaskObserverBridge(id<CRWDownloadTaskObserver> observer);

  DownloadTaskObserverBridge(const DownloadTaskObserverBridge&) = delete;
  DownloadTaskObserverBridge& operator=(const DownloadTaskObserverBridge&) =
      delete;

  ~DownloadTaskObserverBridge() override;

  // web::DownloadTaskObserver methods.
  void OnDownloadUpdated(DownloadTask* task) override;
  void OnDownloadDestroyed(DownloadTask* task) override;

 private:
  __weak id<CRWDownloadTaskObserver> observer_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_OBSERVER_BRIDGE_H_
