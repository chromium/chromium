// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/viz/client/frame_evictor.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/common/content_export.h"

namespace content {

class RenderWidgetHostViewAura;

// DelegatedFrameHostClient implementation for aura, not used in mus.
class CONTENT_EXPORT DelegatedFrameHostClientAura
    : public DelegatedFrameHostClient {
 public:
  explicit DelegatedFrameHostClientAura(
      RenderWidgetHostViewAura* render_widget_host_view);

  DelegatedFrameHostClientAura(const DelegatedFrameHostClientAura&) = delete;
  DelegatedFrameHostClientAura& operator=(const DelegatedFrameHostClientAura&) =
      delete;

  ~DelegatedFrameHostClientAura() override;

 protected:
  RenderWidgetHostViewAura* render_widget_host_view() {
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
  raw_ptr<RenderWidgetHostViewAura> render_widget_host_view_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_
