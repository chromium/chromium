// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_constants_internal.h"

namespace content {

// 20MiB
const size_t kMaxLengthOfDataURLString = 1024 * 1024 * 20;

const int kTraceEventBrowserProcessSortIndex = -6;
const int kTraceEventRendererProcessSortIndex = -5;
const int kTraceEventPpapiProcessSortIndex = -3;
const int kTraceEventPpapiBrokerProcessSortIndex = -2;
const int kTraceEventGpuProcessSortIndex = -1;

const int kTraceEventRendererMainThreadSortIndex = -1;

const char kDoNotTrackHeader[] = "DNT";

const int kChildProcessReceiverAttachmentName = 0;
const int kChildProcessHostRemoteAttachmentName = 1;

} // namespace content
