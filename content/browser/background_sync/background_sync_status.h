// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_STATUS_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_STATUS_H_

namespace content {

// The status of BackgroundSyncManager actions. These are recorded in histograms
// (as BackgroundSyncResult) so don't remove any entries and always append to
// the end.
enum BackgroundSyncStatus {
  BACKGROUND_SYNC_STATUS_OK = 0,
  BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
  BACKGROUND_SYNC_STATUS_NOT_FOUND,
  BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
  BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
  BACKGROUND_SYNC_STATUS_PERMISSION_DENIED,
  BACKGROUND_SYNC_STATUS_MAX = BACKGROUND_SYNC_STATUS_PERMISSION_DENIED
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_STATUS_H_
