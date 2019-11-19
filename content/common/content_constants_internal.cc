// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_constants_internal.h"

#include "build/build_config.h"

namespace content {

#if defined(OS_ANDROID)
const int64_t kHungRendererDelayMs = 5000;
#else
// TODO(jdduke): Consider shortening this delay on desktop. It was originally
// set to 5 seconds but was extended to accomodate less responsive plugins.
const int64_t kHungRendererDelayMs = 30000;
#endif

const int64_t kNewContentRenderingDelayMs = 4000;

const int64_t kAsyncHitTestTimeoutMs = 5000;

// 20MiB
const size_t kMaxLengthOfDataURLString = 1024 * 1024 * 20;

const int kTraceEventBrowserProcessSortIndex = -6;
const int kTraceEventRendererProcessSortIndex = -5;
const int kTraceEventPpapiProcessSortIndex = -3;
const int kTraceEventPpapiBrokerProcessSortIndex = -2;
const int kTraceEventGpuProcessSortIndex = -1;

const int kTraceEventRendererMainThreadSortIndex = -1;

const char kDoNotTrackHeader[] = "DNT";

} // namespace content
