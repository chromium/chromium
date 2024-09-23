// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host_client_android.h"

#include "base/metrics/histogram_macros.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {

DelegatedFrameHostClientAndroid::DelegatedFrameHostClientAndroid(
    RenderWidgetHostViewAndroid* render_widget_host_view)
    : render_widget_host_view_(render_widget_host_view) {
  render_widget_host_view_->host()->AddInputEventObserver(this);
}

DelegatedFrameHostClientAndroid::~DelegatedFrameHostClientAndroid() {
  render_widget_host_view_->host()->RemoveInputEventObserver(this);
}

void DelegatedFrameHostClientAndroid::DidSubmitCompositorFrame() {
  frames_submitted_this_scroll_++;
}

void DelegatedFrameHostClientAndroid::OnInputEvent(
    const blink::WebInputEvent& event) {
  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    frames_submitted_this_scroll_ = 0;
  } else if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd) {
    RecordFrameSubmissionMetrics();
  }
}

void DelegatedFrameHostClientAndroid::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  render_widget_host_view_->OnFrameTokenChangedForView(frame_token,
                                                       activation_time);
}

void DelegatedFrameHostClientAndroid::WasEvicted() {
  render_widget_host_view_->WasEvicted();
}

void DelegatedFrameHostClientAndroid::OnSurfaceIdChanged() {
  render_widget_host_view_->OnSurfaceIdChanged();
}

std::vector<viz::SurfaceId>
DelegatedFrameHostClientAndroid::CollectSurfaceIdsForEviction() const {
  return render_widget_host_view_->host()->CollectSurfaceIdsForEviction();
}

void DelegatedFrameHostClientAndroid::RecordFrameSubmissionMetrics() {
  UMA_HISTOGRAM_COUNTS_100(
      "Event.GestureScrollEnd.BrowserCompositorFrame.Count",
      frames_submitted_this_scroll_);
}

}  // namespace content
