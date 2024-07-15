// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/hit_test_region_observer.h"

#include <algorithm>

#include "base/test/test_timeouts.h"
#include "components/viz/common/hit_test/hit_test_query.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace content {

void WaitForHitTestData(RenderFrameHost* child_frame) {
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderFrameHostImpl*>(child_frame)
          ->GetRenderWidgetHost()
          ->GetView();

  HitTestRegionObserver observer(child_view->GetFrameSinkId());
  observer.WaitForHitTestData();
}

void WaitForHitTestData(WebContents* guest_web_contents) {
  DCHECK(static_cast<RenderWidgetHostViewBase*>(
             guest_web_contents->GetRenderWidgetHostView())
             ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          guest_web_contents->GetRenderWidgetHostView());

  HitTestRegionObserver observer(child_view->GetFrameSinkId());
  observer.WaitForHitTestData();
}

HitTestRegionObserver::HitTestRegionObserver(
    const viz::FrameSinkId& frame_sink_id)
    : frame_sink_id_(frame_sink_id) {
  CHECK(frame_sink_id.is_valid());
  GetHostFrameSinkManager()->AddHitTestRegionObserver(this);
}

HitTestRegionObserver::~HitTestRegionObserver() {
  GetHostFrameSinkManager()->RemoveHitTestRegionObserver(this);
}

void HitTestRegionObserver::WaitForHitTestData() {
  DCHECK(cached_hit_test_data_.empty());

  for (auto& it : GetHostFrameSinkManager()->GetDisplayHitTestQuery()) {
    if (it.second->ContainsActiveFrameSinkId(frame_sink_id_)) {
      cached_hit_test_data_ = it.second->GetHitTestData();
      return;
    }
  }

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void HitTestRegionObserver::WaitForHitTestDataChange() {
  DCHECK(!cached_hit_test_data_.empty());

  for (auto& it : GetHostFrameSinkManager()->GetDisplayHitTestQuery()) {
    DCHECK(it.second->ContainsActiveFrameSinkId(frame_sink_id_));
    if (it.second->GetHitTestData() != cached_hit_test_data_) {
      cached_hit_test_data_ = it.second->GetHitTestData();
      return;
    }
  }

  hit_test_data_change_run_loop_ = std::make_unique<base::RunLoop>();
  hit_test_data_change_run_loop_->Run();
  hit_test_data_change_run_loop_.reset();
}

void HitTestRegionObserver::OnAggregatedHitTestRegionListUpdated(
    const viz::FrameSinkId& frame_sink_id,
    const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) {
  if (hit_test_data_change_run_loop_ &&
      cached_hit_test_data_ != hit_test_data) {
    cached_hit_test_data_ = hit_test_data;
    hit_test_data_change_run_loop_->Quit();
  }

  if (!run_loop_)
    return;

  for (auto& it : hit_test_data) {
    if (it.frame_sink_id == frame_sink_id_ &&
        !(it.flags & viz::HitTestRegionFlags::kHitTestNotActive)) {
      run_loop_->Quit();
      return;
    }
  }
}

const std::vector<viz::AggregatedHitTestRegion>&
HitTestRegionObserver::GetHitTestData() {
  const auto& hit_test_query_map =
      GetHostFrameSinkManager()->GetDisplayHitTestQuery();
  const auto iter = hit_test_query_map.find(frame_sink_id_);
  CHECK(iter != hit_test_query_map.end());
  return iter->second.get()->GetHitTestData();
}

}  // namespace content
