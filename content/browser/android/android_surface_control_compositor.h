// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_ANDROID_SURFACE_CONTROL_COMPOSITOR_H_
#define CONTENT_BROWSER_ANDROID_ANDROID_SURFACE_CONTROL_COMPOSITOR_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/layer_tree_client.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/host/host_display_client.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
class WindowAndroid;
}

namespace content {

// Encapsulates Viz compositor and display lifecycle management for Android
// native overlay windows (e.g. text selection magnifier and unbounded surface
// popups). It handles GPU surface registration, root CompositorFrameSink and
// DisplayPrivate creation, and cc::slim::LayerTree management.
class AndroidSurfaceControlCompositor : public viz::HostDisplayClient,
                                        public viz::HostFrameSinkClient {
 public:
  explicit AndroidSurfaceControlCompositor(viz::FrameSinkId frame_sink_id);
  ~AndroidSurfaceControlCompositor() override;

  bool Initialize(ui::WindowAndroid& window_android,
                  const base::android::JavaRef<jobject>& j_surface_control,
                  cc::slim::LayerTreeClient* client,
                  scoped_refptr<cc::slim::Layer> root_layer,
                  const gfx::Size& size_pixels,
                  float device_scale_factor);

  void Resize(const gfx::Size& size_pixels,
              float device_scale_factor,
              const viz::LocalSurfaceId& local_surface_id);

  // viz::mojom::DisplayClient implementation:
  void DidCompleteSwapWithSize(const gfx::Size& pixel_size) override {}
  void OnContextCreationResult(gpu::ContextResult context_result) override {}
  void SetWideColorEnabled(bool enabled) override {}
  void SetPreferredRefreshRate(float refresh_rate) override {}

  // viz::HostFrameSinkClient implementation:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
  }
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override {}

 private:
  bool CreateDisplayAndFrameSink(ui::WindowAndroid& window_android,
                                 const gfx::Size& size_pixels,
                                 float device_scale_factor);

  const viz::FrameSinkId frame_sink_id_;

  bool is_registered_ = false;
  gpu::SurfaceHandle surface_handle_ = gpu::kNullSurfaceHandle;
  mojo::AssociatedRemote<viz::mojom::DisplayPrivate> display_private_;
  std::unique_ptr<cc::slim::LayerTree> layer_tree_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_ANDROID_SURFACE_CONTROL_COMPOSITOR_H_
