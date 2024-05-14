// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_ANDROID_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/android/delegated_frame_host_android.h"

namespace content {

class RenderWidgetHostViewAndroid;

class CONTENT_EXPORT DelegatedFrameHostClientAndroid
    : public RenderWidgetHost::InputEventObserver,
      public ui::DelegatedFrameHostAndroid::Client {
 public:
  explicit DelegatedFrameHostClientAndroid(
      RenderWidgetHostViewAndroid* render_widget_host_view);

  DelegatedFrameHostClientAndroid(const DelegatedFrameHostClientAndroid&) =
      delete;
  DelegatedFrameHostClientAndroid& operator=(
      const DelegatedFrameHostClientAndroid&) = delete;

  ~DelegatedFrameHostClientAndroid() override;

  void DidSubmitCompositorFrame() override;
  void OnInputEvent(const blink::WebInputEvent& event) override;

 private:
  // DelegatedFrameHostAndroid::Client implementation.
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;
  void WasEvicted() override;
  void OnSurfaceIdChanged() override;
  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction() const override;
  void RecordFrameSubmissionMetrics();

  raw_ptr<RenderWidgetHostViewAndroid> render_widget_host_view_;

  int frames_submitted_this_scroll_ = 0u;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_ANDROID_H_
