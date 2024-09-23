// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_

#include <stdint.h>

#include "content/public/renderer/render_frame_observer.h"
#include "extensions/common/mojom/app_window.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace extensions {

// This class holds the extensions specific parts of RenderFrame, and has the
// same lifetime.
class ExtensionsRenderFrameObserver : public content::RenderFrameObserver,
                                      public mojom::AppWindow {
 public:
  ExtensionsRenderFrameObserver(content::RenderFrame* render_frame,
                                service_manager::BinderRegistry* registry);
  ExtensionsRenderFrameObserver(const ExtensionsRenderFrameObserver&) = delete;
  ExtensionsRenderFrameObserver& operator=(
      const ExtensionsRenderFrameObserver&) = delete;
  ~ExtensionsRenderFrameObserver() override;

 private:
  void BindAppWindowReceiver(mojo::PendingReceiver<mojom::AppWindow> receiver);

  // Toggles visual muting of the render view area. This is on when a
  // constrained window is showing.
  void SetVisuallyDeemphasized(bool deemphasized) override;
  void SetSupportsDraggableRegions(bool supports_draggable_regions) override;

  // RenderFrameObserver implementation.
  void DetailedConsoleMessageAdded(
      const std::u16string& message,
      const std::u16string& source,
      const std::u16string& stack_trace,
      uint32_t line_number,
      blink::mojom::ConsoleMessageLevel level) override;
  void OnDestruct() override;

  // true if webview is overlayed with grey color.
  bool webview_visually_deemphasized_ = false;

  mojo::ReceiverSet<mojom::AppWindow> receivers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_

