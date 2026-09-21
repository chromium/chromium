// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_INOTIFY_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_INOTIFY_H_

#include <stddef.h>

#include "base/functional/function_ref.h"
#include "content/common/content_export.h"

struct inotify_event;

namespace content {

// Drains queued events from `inotify_fd` through the production reader loop.
// Tests can supply a packet socket to control read boundaries. `dispatch_event`
// receives either a single event (second argument null) or a matched move pair.
CONTENT_EXPORT bool ReadInotifyEventsForTesting(
    int inotify_fd,
    base::FunctionRef<void(const inotify_event*, const inotify_event*)>
        dispatch_event);

CONTENT_EXPORT size_t
GetQuotaLimitFromSystemLimitForTesting(size_t system_limit);

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_PATH_WATCHER_FILE_PATH_WATCHER_INOTIFY_H_
