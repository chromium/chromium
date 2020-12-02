// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_H_
#define CONTENT_RENDERER_RENDER_WIDGET_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/touch_action.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/managed_memory_policy.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/renderer.mojom-forward.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/mojom/page/record_content_to_visible_time_request.mojom-forward.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

namespace blink {
class WebFrameWidget;
}  // namespace blink

namespace content {
class CompositorDependencies;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
//
// RenderWidget is used to implement:
// - RenderViewImpl (deprecated)
// - Widgets for frames (the main frame, and subframes due to out-of-process
//   iframe support)
//
// Background info:
// OOPIF causes webpages to be renderered by multiple renderers. Each renderer
// has one instance of a RenderViewImpl, which represents page state shared by
// each renderer. The frame tree is mirrored across each renderer. Local nodes
// are represented by RenderFrame, and remote nodes are represented by
// RenderFrameProxy. Each local root has a corresponding RenderWidget. This
// RenderWidget is used to route input and graphical output between the browser
// and the renderer.
class CONTENT_EXPORT RenderWidget : public blink::WebWidgetClient {
 public:
  RenderWidget(base::PassKey<RenderWidget>,
               CompositorDependencies* compositor_deps);

  ~RenderWidget() override;

  // Creates a RenderWidget that is meant to be associated with a RenderFrame.
  static std::unique_ptr<RenderWidget> CreateForFrame(
      CompositorDependencies* compositor_deps);

  // Initialize a new RenderWidget that will be attached to a RenderFrame (via
  // the WebFrameWidget), for a frame that is a main frame.
  void InitForMainFrame(blink::WebFrameWidget* web_frame_widget,
                        const blink::ScreenInfo& screen_info);

  // Initialize a new RenderWidget that will be attached to a RenderFrame (via
  // the WebFrameWidget), for a frame that is a local root, but not the main
  // frame.
  void InitForChildLocalRoot(blink::WebFrameWidget* web_frame_widget,
                             const blink::ScreenInfo& screen_info);

  // Closes a RenderWidget that was created by |CreateForFrame|. Ownership is
  // passed into this object to asynchronously delete itself.
  void CloseForFrame(std::unique_ptr<RenderWidget> widget);

  cc::LayerTreeHost* layer_tree_host() { return layer_tree_host_; }

 private:
  // Destroy the RenderWidget. The |widget| is the owning pointer of |this|.
  void Close(std::unique_ptr<RenderWidget> widget);

  void Initialize(blink::WebWidget* web_widget,
                  const blink::ScreenInfo& screen_info);

  // Initializes the compositor and dependent systems, as part of the
  // Initialize() process.
  void InitCompositing(const blink::ScreenInfo& screen_info);

  // Dependencies for initializing a compositor, including flags for optional
  // features.
  CompositorDependencies* const compositor_deps_;

  // We are responsible for destroying this object via its Close method, unless
  // the RenderWidget is associated with a RenderViewImpl through |delegate_|.
  // Becomes null once close is initiated on the RenderWidget.
  blink::WebWidget* webwidget_ = nullptr;

  // This is valid while |webwidget_| is valid.
  cc::LayerTreeHost* layer_tree_host_ = nullptr;

  // True once Close() is called, during the self-destruction process, and to
  // verify destruction always goes through Close().
  bool closing_ = false;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_
