// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_
#define CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/public/browser/render_widget_host_view.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace blink {
class WebMouseEvent;
}

namespace content {

class UnboundedSurfaceWindow {
 public:
  virtual ~UnboundedSurfaceWindow() = default;

  virtual bool is_valid() const = 0;
  virtual gfx::NativeWindow GetNativeWindow() const = 0;
  virtual void Dismiss() = 0;

  virtual void SetBounds(const gfx::Rect& bounds_in_screen) = 0;
  virtual viz::FrameSinkId GetFrameSinkId() const = 0;
  virtual viz::LocalSurfaceId GetLocalSurfaceId() const = 0;
  virtual void GetCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client) = 0;

  virtual void RouteMouseEvent(const blink::WebMouseEvent& event) = 0;
  virtual gfx::Rect GetBounds() const = 0;
  virtual void CopyFromSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      base::TimeDelta timeout,
      base::OnceCallback<void(const content::CopyFromSurfaceResult&)>
          callback) = 0;
  virtual base::WeakPtr<UnboundedSurfaceWindow> GetWeakPtr() = 0;
  virtual void EnsureSurfaceSynchronizedForWebTest() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_UNBOUNDED_SURFACE_WINDOW_H_
