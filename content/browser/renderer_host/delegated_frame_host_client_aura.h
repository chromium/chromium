// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_

#include "base/macros.h"
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
  ~DelegatedFrameHostClientAura() override;

 protected:
  RenderWidgetHostViewAura* render_widget_host_view() {
    return render_widget_host_view_;
  }

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  SkColor DelegatedFrameHostGetGutterColor() const override;
  void OnFrameTokenChanged(uint32_t frame_token) override;
  float GetDeviceScaleFactor() const override;
  void InvalidateLocalSurfaceIdOnEviction() override;
  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction() override;
  bool ShouldShowStaleContentOnEviction() override;

 private:
  RenderWidgetHostViewAura* render_widget_host_view_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHostClientAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_AURA_H_
