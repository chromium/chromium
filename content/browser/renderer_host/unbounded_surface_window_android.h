// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/layer_tree_client.h"
#include "cc/slim/surface_layer.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/android/android_surface_control_compositor.h"
#include "content/browser/renderer_host/unbounded_surface_window.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/unbounded_element/unbounded_element.mojom.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class RenderWidgetHostViewAndroid;
class RenderWidgetHostViewBase;

class UnboundedSurfaceWindowAndroid : public UnboundedSurfaceWindow,
                                      public viz::HostFrameSinkClient,
                                      public cc::slim::LayerTreeClient {
 public:
  static std::unique_ptr<UnboundedSurfaceWindowAndroid> Create(
      RenderWidgetHostViewAndroid* parent_view,
      mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
      mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient>
          client,
      const gfx::Rect& bounds_in_dips,
      base::WeakPtr<RenderWidgetHostViewBase> subframe_view);

  ~UnboundedSurfaceWindowAndroid() override;

  // UnboundedSurfaceWindow overrides:
  base::WeakPtr<UnboundedSurfaceWindow> GetWeakPtr() override;
  bool IsValid() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  void SetBounds(const gfx::Rect& bounds_in_dips) override;
  viz::FrameSinkId GetFrameSinkId() const override;
  viz::LocalSurfaceId GetLocalSurfaceId() const override;
  void GetCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client)
      override;

  void RouteMouseEvent(const blink::WebMouseEvent& event) override;
  gfx::Rect GetBounds() const override;
  void CopyFromSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      base::TimeDelta timeout,
      base::OnceCallback<void(const content::CopyFromSurfaceResult&)> callback)
      override;
  void EnsureSurfaceSynchronizedForWebTest() override;
  // blink::mojom::UnboundedSurfaceHost overrides:
  void UpdateBounds(const gfx::Rect& bounds) override;

  // viz::HostFrameSinkClient overrides:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {
  }
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override {}

  // cc::slim::LayerTreeClient overrides:
  void BeginFrame(const viz::BeginFrameArgs& args) override {}
  void DidReceiveCompositorFrameAck() override {}
  void RequestNewFrameSink() override {}
  void DidInitializeLayerTreeFrameSink() override {}
  void DidFailToInitializeLayerTreeFrameSink() override;
  void DidSubmitCompositorFrame() override {}
  void DidLoseLayerTreeFrameSink() override;

 protected:
  void TeardownAndDestroy() override;

 private:
  UnboundedSurfaceWindowAndroid(
      RenderWidgetHostViewAndroid* parent_view,
      mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
      mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient>
          client,
      base::WeakPtr<RenderWidgetHostViewBase> subframe_view);

  bool InitWindow(const gfx::Rect& bounds_in_dips);
  void OnConnectionError();

  base::WeakPtr<RenderWidgetHostViewAndroid> parent_view_;
  base::WeakPtr<RenderWidgetHostViewBase> subframe_view_;
  viz::FrameSinkId root_frame_sink_id_;
  viz::FrameSinkId client_frame_sink_id_;
  viz::ParentLocalSurfaceIdAllocator root_local_surface_id_allocator_;
  viz::ParentLocalSurfaceIdAllocator client_local_surface_id_allocator_;
  mojo::AssociatedReceiver<blink::mojom::UnboundedSurfaceHost> receiver_{this};

  raw_ptr<ui::WindowAndroid> window_android_ = nullptr;
  std::unique_ptr<AndroidSurfaceControlCompositor> compositor_;
  scoped_refptr<cc::slim::SurfaceLayer> surface_layer_;

  base::android::ScopedJavaGlobalRef<jobject> j_popup_window_;
  gfx::Rect bounds_in_dips_;

  base::WeakPtrFactory<UnboundedSurfaceWindowAndroid> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_ANDROID_H_
