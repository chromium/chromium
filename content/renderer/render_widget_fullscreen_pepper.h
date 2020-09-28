// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
#define CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/pepper/fullscreen_container.h"
#include "content/renderer/render_widget.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/page/widget.mojom.h"
#include "third_party/blink/public/web/web_external_widget.h"
#include "third_party/blink/public/web/web_external_widget_client.h"
#include "url/gurl.h"

namespace cc {
class Layer;
}

namespace content {
class AgentSchedulingGroup;
class CompositorDependencies;
class PepperPluginInstanceImpl;
class PepperExternalWidgetClient;

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper : public RenderWidget,
                                     public FullscreenContainer {
 public:
  // The created object is owned by the browser process. The browser process
  // is responsible for destroying it with an IPC message.
  static RenderWidgetFullscreenPepper* Create(
      AgentSchedulingGroup& agent_scheduling_group,
      int32_t routing_id,
      RenderWidget::ShowCallback show_callback,
      CompositorDependencies* compositor_deps,
      const blink::ScreenInfo& screen_info,
      PepperPluginInstanceImpl* plugin,
      const blink::WebURL& local_main_frame_url,
      mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> blink_widget_host,
      mojo::PendingAssociatedReceiver<blink::mojom::Widget> blink_widget);

  // pepper::FullscreenContainer API.
  void Destroy() override;
  void PepperDidChangeCursor(const ui::Cursor& cursor) override;
  void SetLayer(scoped_refptr<cc::Layer> layer) override;

  // Could be NULL when this widget is closing.
  PepperPluginInstanceImpl* plugin() const { return plugin_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

 protected:
  RenderWidgetFullscreenPepper(
      AgentSchedulingGroup& agent_scheduling_group,
      int32_t routing_id,
      CompositorDependencies* compositor_deps,
      PepperPluginInstanceImpl* plugin,
      mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> blink_widget_host,
      mojo::PendingAssociatedReceiver<blink::mojom::Widget> blink_widget,
      blink::WebURL main_frame_url);
  ~RenderWidgetFullscreenPepper() override;

  // RenderWidget API.
  void Close(std::unique_ptr<RenderWidget> widget) override;

 private:
  friend class PepperExternalWidgetClient;

  void DidInitiatePaint();
  void UpdateLayerBounds();
  void DidResize(const gfx::Size& size);
  blink::WebInputEventResult ProcessInputEvent(
      const blink::WebCoalescedInputEvent& event);

  // The plugin instance this widget wraps.
  PepperPluginInstanceImpl* plugin_;

  cc::Layer* layer_ = nullptr;

  std::unique_ptr<MouseLockDispatcher> mouse_lock_dispatcher_;
  std::unique_ptr<PepperExternalWidgetClient> widget_client_;
  std::unique_ptr<blink::WebExternalWidget> blink_widget_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
