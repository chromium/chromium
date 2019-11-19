// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

// How long to wait before we consider a renderer hung.
CONTENT_EXPORT extern const int64_t kHungRendererDelayMs;

// How long to wait for newly loaded content to send a compositor frame
// before clearing previously displayed graphics.
extern const int64_t kNewContentRenderingDelayMs;

// Maximum wait time for an asynchronous hit test request sent to a renderer
// process (in milliseconds).
CONTENT_EXPORT extern const int64_t kAsyncHitTestTimeoutMs;

// The maximum length of string as data url.
extern const size_t kMaxLengthOfDataURLString;

// Constants used to organize content processes in about:tracing.
CONTENT_EXPORT extern const int kTraceEventBrowserProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventRendererProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventPpapiProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventPpapiBrokerProcessSortIndex;
CONTENT_EXPORT extern const int kTraceEventGpuProcessSortIndex;

// Constants used to organize content threads in about:tracing.
CONTENT_EXPORT extern const int kTraceEventRendererMainThreadSortIndex;

// HTTP header set in requests to indicate they should be marked DoNotTrack.
extern const char kDoNotTrackHeader[];

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
