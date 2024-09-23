// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SYNCHRONIZE_VISUAL_PROPERTIES_INTERCEPTOR_H_
#define CONTENT_PUBLIC_TEST_SYNCHRONIZE_VISUAL_PROPERTIES_INTERCEPTOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom-test-utils.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class RenderFrameProxyHost;

// Class to intercept SynchronizeVisualProperties method. This allows the
// message to continue to the target child so that processing can be
// verified by tests. It also monitors for GesturePinchBegin/End events.
class SynchronizeVisualPropertiesInterceptor
    : public blink::mojom::RemoteFrameHostInterceptorForTesting {
 public:
  explicit SynchronizeVisualPropertiesInterceptor(
      RenderFrameProxyHost* render_frame_proxy_host);

  SynchronizeVisualPropertiesInterceptor(
      const SynchronizeVisualPropertiesInterceptor&) = delete;
  SynchronizeVisualPropertiesInterceptor& operator=(
      const SynchronizeVisualPropertiesInterceptor&) = delete;

  ~SynchronizeVisualPropertiesInterceptor() override;

  blink::mojom::RemoteFrameHost* GetForwardingInterface() override;

  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) override;

  gfx::Rect last_rect() const { return last_rect_; }

  void WaitForRect();
  void ResetRectRunLoop();

  // Waits for the next viz::LocalSurfaceId be received and returns it.
  viz::LocalSurfaceId WaitForSurfaceId();

  void WaitForPinchGestureEnd();

 private:
  // |rect| is in DIPs.
  void OnUpdatedFrameRectOnUI(const gfx::Rect& rect);
  void OnUpdatedFrameSinkIdOnUI();
  void OnUpdatedSurfaceIdOnUI(viz::LocalSurfaceId surface_id);

  base::RunLoop run_loop_;

  std::unique_ptr<base::RunLoop> local_root_rect_run_loop_;
  bool local_root_rect_received_ = false;
  gfx::Rect last_rect_;

  viz::LocalSurfaceId last_surface_id_;
  std::unique_ptr<base::RunLoop> surface_id_run_loop_;

  bool last_pinch_gesture_active_ = false;
  base::RunLoop pinch_end_run_loop_;

  mojo::test::ScopedSwapImplForTesting<blink::mojom::RemoteFrameHost>
      swapped_impl_;

  base::WeakPtrFactory<SynchronizeVisualPropertiesInterceptor> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SYNCHRONIZE_VISUAL_PROPERTIES_INTERCEPTOR_H_
