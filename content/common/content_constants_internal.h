// Copyright 2012 The Chromium Authors
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

#if BUILDFLAG(IS_ANDROID)
// The mobile hang timer is shorter than the desktop hang timer because the
// screen is smaller and more intimate, and therefore requires more nimbleness.
constexpr base::TimeDelta kHungRendererDelay = base::Seconds(5);
#else
// It would be nice to lower the desktop delay, but going any further with the
// modal dialog UI would be disruptive, and while new gentle UI indicating that
// a page is hung would be great, that UI isn't going to happen any time soon.
constexpr base::TimeDelta kHungRendererDelay = base::Seconds(15);
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

// Accept header used for frame requests.
CONTENT_EXPORT extern const char kFrameAcceptHeaderValue[];

// Constants for attaching message pipes to the mojo invitation used to
// initialize child processes.
extern const int kChildProcessReceiverAttachmentName;
extern const int kChildProcessHostRemoteAttachmentName;
extern const int kLegacyIpcBootstrapAttachmentName;

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
