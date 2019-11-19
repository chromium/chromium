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
#include "third_party/blink/public/web/web_widget.h"
#include "url/gurl.h"

namespace cc {
class Layer;
}

namespace content {
class CompositorDependencies;
class PepperPluginInstanceImpl;

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper : public RenderWidget,
                                     public FullscreenContainer {
 public:
  // The created object is owned by the browser process. The browser process
  // is responsible for destroying it with an IPC message.
  static RenderWidgetFullscreenPepper* Create(
      int32_t routing_id,
      RenderWidget::ShowCallback show_callback,
      CompositorDependencies* compositor_deps,
      const ScreenInfo& screen_info,
      PepperPluginInstanceImpl* plugin,
      const blink::WebURL& local_main_frame_url,
      mojo::PendingReceiver<mojom::Widget> widget_receiver);

  // pepper::FullscreenContainer API.
  void ScrollRect(int dx, int dy, const blink::WebRect& rect) override;
  void Destroy() override;
  void PepperDidChangeCursor(const blink::WebCursorInfo& cursor) override;
  void SetLayer(cc::Layer* layer) override;

  // RenderWidget overrides.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Could be NULL when this widget is closing.
  PepperPluginInstanceImpl* plugin() const { return plugin_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

 protected:
  RenderWidgetFullscreenPepper(
      int32_t routing_id,
      CompositorDependencies* compositor_deps,
      PepperPluginInstanceImpl* plugin,
      mojo::PendingReceiver<mojom::Widget> widget_receiver);
  ~RenderWidgetFullscreenPepper() override;

  // RenderWidget API.
  void DidInitiatePaint() override;
  void Close(std::unique_ptr<RenderWidget> widget) override;
  void AfterUpdateVisualProperties() override;

 private:
  void UpdateLayerBounds();

  // The plugin instance this widget wraps.
  PepperPluginInstanceImpl* plugin_;

  cc::Layer* layer_ = nullptr;

  std::unique_ptr<MouseLockDispatcher> mouse_lock_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
