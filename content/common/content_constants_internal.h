// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

#if defined(OS_ANDROID)
constexpr base::TimeDelta kHungRendererDelay = base::TimeDelta::FromSeconds(5);
#else
// TODO(jdduke): Consider shortening this delay on desktop. It was originally
// set to 5 seconds but was extended to accommodate less responsive plugins.
constexpr base::TimeDelta kHungRendererDelay = base::TimeDelta::FromSeconds(30);
#endif

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

// Constants for attaching message pipes to the mojo invitation used to
// initialize child processes.
extern const int kChildProcessReceiverAttachmentName;
extern const int kChildProcessHostRemoteAttachmentName;

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
