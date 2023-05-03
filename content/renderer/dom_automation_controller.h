// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_AUTOMATION_CONTROLLER_H_
#define CONTENT_RENDERER_DOM_AUTOMATION_CONTROLLER_H_

#include <stdint.h>

#include "content/common/dom_automation_controller.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace blink {
class WebLocalFrame;
}

namespace gin {
class Arguments;
}

namespace content {

class RenderFrame;

// Provides implementation of window.domAutomationController javascript object.
// Javascript can call domAutomationController.send(...) to send arbitrary data
// to the browser.  On the browser side, the data is received via the
// DOMMessageQueue class.
class DomAutomationController : public gin::Wrappable<DomAutomationController>,
                                public RenderFrameObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  DomAutomationController(const DomAutomationController&) = delete;
  DomAutomationController& operator=(const DomAutomationController&) = delete;

  static void Install(RenderFrame* render_frame, blink::WebLocalFrame* frame);

  // Makes the renderer send a javascript value to the app.
  // The value to be sent can be either of type String,
  // Number (double casted to int32_t) or Boolean. Any other type or no
  // argument at all is ignored.
  bool SendMsg(const gin::Arguments& args);

 private:
  explicit DomAutomationController(RenderFrame* render_view);
  ~DomAutomationController() override;

  // gin::WrappableBase
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // RenderFrameObserver
  void OnDestruct() override;
  void DidCreateScriptContext(v8::Local<v8::Context> context,
                              int32_t world_id) override;

  const mojo::AssociatedRemote<mojom::DomAutomationControllerHost>&
  GetDomAutomationControllerHost();

  mojo::AssociatedRemote<mojom::DomAutomationControllerHost>
      dom_automation_controller_host_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_AUTOMATION_CONTROLLER_H_
