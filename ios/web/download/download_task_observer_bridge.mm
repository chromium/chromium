// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/download/download_task_observer_bridge.h"

namespace web {

DownloadTaskObserverBridge::DownloadTaskObserverBridge(
    id<CRWDownloadTaskObserver> observer)
    : observer_(observer) {}

DownloadTaskObserverBridge::~DownloadTaskObserverBridge() = default;

void DownloadTaskObserverBridge::OnDownloadUpdated(DownloadTask* task) {
  if ([observer_ respondsToSelector:@selector(downloadUpdated:)]) {
    [observer_ downloadUpdated:task];
  }
}

void DownloadTaskObserverBridge::OnDownloadDestroyed(DownloadTask* task) {
  if ([observer_ respondsToSelector:@selector(downloadDestroyed:)]) {
    [observer_ downloadDestroyed:task];
  }
}

}  // namespace web
