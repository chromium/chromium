// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_INOTIFY_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_INOTIFY_H_

#include <stddef.h>

#include "content/common/content_export.h"

namespace content {

// Get the maximum number of inotify watches can be used by a FilePathWatcher
// instance. This is based on /proc/sys/fs/inotify/max_user_watches entry.
CONTENT_EXPORT size_t GetMaxNumberOfInotifyWatches();

// Overrides max inotify watcher counter for test.
class CONTENT_EXPORT ScopedMaxNumberOfInotifyWatchesOverrideForTest {
 public:
  explicit ScopedMaxNumberOfInotifyWatchesOverrideForTest(size_t override_max);
  ~ScopedMaxNumberOfInotifyWatchesOverrideForTest();
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_INOTIFY_H_
