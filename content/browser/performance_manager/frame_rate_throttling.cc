// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/frame_rate_throttling.h"

#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/public/browser/browser_thread.h"

namespace content {

CONTENT_EXPORT void StartThrottlingAllFrameSinks(base::TimeDelta interval) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetHostFrameSinkManager()->StartThrottlingAllFrameSinks(interval);
}

CONTENT_EXPORT void StopThrottlingAllFrameSinks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetHostFrameSinkManager()->StopThrottlingAllFrameSinks();
}

}  // namespace content
