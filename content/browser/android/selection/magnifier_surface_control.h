// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SELECTION_MAGNIFIER_SURFACE_CONTROL_H_
#define CONTENT_BROWSER_ANDROID_SELECTION_MAGNIFIER_SURFACE_CONTROL_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree_client.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/android/android_surface_control_compositor.h"

namespace content {

class WebContentsImpl;

class MagnifierSurfaceControl : public cc::slim::LayerTreeClient {
 public:
  MagnifierSurfaceControl(
      WebContentsImpl* web_contents,
      const base::android::JavaRef<jobject>& j_surface_control,
      float device_scale,
      int width,
      int height,
      float corner_radius,
      float zoom,
      int top_shadow_height,
      int bottom_shadow_height,
      int bottom_shadow_width_reduction);
  ~MagnifierSurfaceControl() override;

  void SetReadbackOrigin(JNIEnv* env, float x, float y);
  void ChildLocalSurfaceIdChanged(JNIEnv* env);

  // cc::slim::LayerTreeClient implementation:
  void BeginFrame(const viz::BeginFrameArgs& args) override {}
  void DidReceiveCompositorFrameAck() override {}
  void RequestNewFrameSink() override {}
  void DidInitializeLayerTreeFrameSink() override {}
  void DidFailToInitializeLayerTreeFrameSink() override {}
  void DidSubmitCompositorFrame() override {}
  void DidLoseLayerTreeFrameSink() override {}

 private:
  void UpdateLayers();

  const raw_ptr<WebContentsImpl> web_contents_;
  const viz::FrameSinkId frame_sink_id_;

  const gfx::Size surface_size_;  // Includes shadow.
  const scoped_refptr<cc::slim::Layer> root_layer_;
  const scoped_refptr<cc::slim::SolidColorLayer> rounded_corner_layer_;
  const scoped_refptr<cc::slim::Layer> zoom_layer_;
  const scoped_refptr<cc::slim::SurfaceLayer> surface_layer_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;

  std::unique_ptr<AndroidSurfaceControlCompositor> compositor_;

  float readback_origin_x_ = 0.f;
  float readback_origin_y_ = 0.f;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SELECTION_MAGNIFIER_SURFACE_CONTROL_H_
