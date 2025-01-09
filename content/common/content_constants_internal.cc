// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_constants_internal.h"

#include "media/media_buildflags.h"

namespace content {

// 20MiB
const size_t kMaxLengthOfDataURLString = 1024 * 1024 * 20;

const int kTraceEventBrowserProcessSortIndex = -6;
const int kTraceEventRendererProcessSortIndex = -5;
const int kTraceEventPpapiProcessSortIndex = -3;
const int kTraceEventPpapiBrokerProcessSortIndex = -2;
const int kTraceEventGpuProcessSortIndex = -1;

const int kTraceEventRendererMainThreadSortIndex = -1;

#if BUILDFLAG(ENABLE_AV1_DECODER)
const char kFrameAcceptHeaderValue[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,"
    "image/webp,image/apng,*/*;q=0.8";
#else
const char kFrameAcceptHeaderValue[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
    "image/apng,*/*;q=0.8";
#endif

const int kChildProcessReceiverAttachmentName = 0;
const int kChildProcessHostRemoteAttachmentName = 1;
const int kLegacyIpcBootstrapAttachmentName = 2;

} // namespace content
