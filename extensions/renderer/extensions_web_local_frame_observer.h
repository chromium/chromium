// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_WEB_LOCAL_FRAME_OBSERVER_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_WEB_LOCAL_FRAME_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "extensions/common/mojom/app_window.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/web/web_local_frame_observer.h"

namespace blink {
class WebLocalFrame;
class WebString;
struct WebConsoleMessage;
}  // namespace blink

namespace extensions {

// This class holds the extensions specific parts of WebLocalFrame, and has the
// same lifetime.
class ExtensionsWebLocalFrameObserver : public blink::WebLocalFrameObserver,
                                        public mojom::AppWindow {
 public:
  ExtensionsWebLocalFrameObserver(blink::WebLocalFrame* web_local_frame,
                                  service_manager::BinderRegistry* registry);
  ExtensionsWebLocalFrameObserver(const ExtensionsWebLocalFrameObserver&) =
      delete;
  ExtensionsWebLocalFrameObserver& operator=(
      const ExtensionsWebLocalFrameObserver&) = delete;
  ~ExtensionsWebLocalFrameObserver() override;

 private:
  void BindAppWindowReceiver(mojo::PendingReceiver<mojom::AppWindow> receiver);

  // mojom::AppWindow implementation:
  // Toggles visual muting of the render view area. This is on when a
  // constrained window is showing.
  void SetVisuallyDeemphasized(bool deemphasized) override;

  // blink::WebLocalFrameObserver implementation:
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void OnFrameDetached() override;

  // true if webview is overlaid with grey color.
  bool webview_visually_deemphasized_ = false;

  mojo::ReceiverSet<mojom::AppWindow> receivers_;
  base::WeakPtrFactory<ExtensionsWebLocalFrameObserver> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_WEB_LOCAL_FRAME_OBSERVER_H_
