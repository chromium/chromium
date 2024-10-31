// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/frame_rate_throttling.h"

#include <vector>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"

namespace content {

CONTENT_EXPORT void StartThrottlingAllFrameSinks(base::TimeDelta interval) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetHostFrameSinkManager()->StartThrottlingAllFrameSinks(interval);
}

CONTENT_EXPORT void StopThrottlingAllFrameSinks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetHostFrameSinkManager()->StopThrottlingAllFrameSinks();
}

CONTENT_EXPORT void UpdateThrottlingFrameSinks(
    const std::set<GlobalRenderFrameHostId>& throttle_frames,
    base::TimeDelta interval) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // viz::HostFrameSinkManager is not available in some unittest.
  if (!GetHostFrameSinkManager()) {
    return;
  }

  std::set<viz::FrameSinkId> sink_ids;
  for (const GlobalRenderFrameHostId& frame_id : throttle_frames) {
    // If frame is deleted, RenderFrameHost no longer exists.
    auto* rfh = content::RenderFrameHost::FromID(frame_id);
    auto* rwh = rfh ? rfh->GetRenderWidgetHost() : nullptr;
    auto sink_id = rwh ? rwh->GetFrameSinkId() : viz::FrameSinkId();
    if (sink_id.is_valid()) {
      sink_ids.insert(std::move(sink_id));
    }
  }
  std::vector<viz::FrameSinkId> sink_ids_vector(sink_ids.begin(),
                                                sink_ids.end());
  GetHostFrameSinkManager()->Throttle(sink_ids_vector, interval);
}

}  // namespace content
