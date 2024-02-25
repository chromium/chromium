// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/download/download_task_observer.h"

#import <ostream>

#import "base/check.h"

namespace web {

DownloadTaskObserver::~DownloadTaskObserver() {
  CHECK(!IsInObserverList())
      << "DownloadTaskObserver needs to be removed from DownloadTask observer "
         "list before their destruction.";
}

}  // namespace web
