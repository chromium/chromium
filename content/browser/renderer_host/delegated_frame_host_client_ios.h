// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_IOS_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_IOS_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/viz/client/frame_evictor.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/common/content_export.h"

namespace content {

class RenderWidgetHostViewIOS;

// DelegatedFrameHostClient implementation for iOS.
class CONTENT_EXPORT DelegatedFrameHostClientIOS
    : public DelegatedFrameHostClient {
 public:
  explicit DelegatedFrameHostClientIOS(
      RenderWidgetHostViewIOS* render_widget_host_view);

  DelegatedFrameHostClientIOS(const DelegatedFrameHostClientIOS&) = delete;
  DelegatedFrameHostClientIOS& operator=(const DelegatedFrameHostClientIOS&) =
      delete;

  ~DelegatedFrameHostClientIOS() override;

 protected:
  RenderWidgetHostViewIOS* render_widget_host_view() {
    return render_widget_host_view_;
  }

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  SkColor DelegatedFrameHostGetGutterColor() const override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;
  float GetDeviceScaleFactor() const override;
  void InvalidateLocalSurfaceIdOnEviction() override;
  viz::FrameEvictorClient::EvictIds CollectSurfaceIdsForEviction() override;
  bool ShouldShowStaleContentOnEviction() override;

 private:
  raw_ptr<RenderWidgetHostViewIOS> render_widget_host_view_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_IOS_H_
