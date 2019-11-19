// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_
#define CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/renderer/render_view_observer.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/range/range.h"

namespace blink {
class WebFrameWidget;
class WebLocalFrame;
}

namespace content {

class PepperPluginInstanceImpl;
class RenderWidget;

// This is the renderer-side message filter that generates the replies for the
// messages sent by the TextInputClientMac. See
// content/browser/renderer_host/text_input_client_mac.h for more information.
class TextInputClientObserver : public IPC::Listener, public IPC::Sender {
 public:
  // Pass a null RenderWidget when the TextInputClientObserver is for a
  // RenderWidget not associated with a RenderFrame. The TextInputClientObserver
  // expects that the RenderWidget's WebWidget will always be a WebFrameWidget.
  // When given a null, the TextInputClientObserver can still reply to IPC
  // messages with empty results.
  explicit TextInputClientObserver(RenderWidget* render_widget);
  ~TextInputClientObserver() override;

  // IPC::Listener override.
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender override.
  bool Send(IPC::Message* message) override;

 private:
  // The WebFrameWidget corresponding to this TextInputClientObserver. While the
  // main frame is remote, the value returned from this method for a main frame
  // RenderWidget is null.
  // TODO(crbug.com/669219): The browser shouldn't be sending IPCs that land
  // in this class when the main frame is remote.
  // TODO(crbug.com/419087): The lifetime of the WebFrameWidget should
  // eventually be tied to the lifetime of the RenderWidget so this would never
  // return null.
  blink::WebFrameWidget* GetWebFrameWidget() const;

  blink::WebLocalFrame* GetFocusedFrame() const;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Returns the currently focused pepper plugin on the page. The expectation is
  // that the focused pepper plugin is inside a frame whose local root is equal
  // to GetWebFrameWidget()->localRoot().
  PepperPluginInstanceImpl* GetFocusedPepperPlugin() const;
#endif

  // IPC Message handlers:
  void OnStringAtPoint(gfx::Point point);
  void OnCharacterIndexForPoint(gfx::Point point);
  void OnFirstRectForCharacterRange(gfx::Range range);
  void OnStringForRange(gfx::Range range);

  // The RenderWidget owning this instance of the observer.
  RenderWidget* render_widget_;

  DISALLOW_COPY_AND_ASSIGN(TextInputClientObserver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_
