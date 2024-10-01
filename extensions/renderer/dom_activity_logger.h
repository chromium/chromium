// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_
#define EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_

#include "base/values.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/web/web_dom_activity_logger.h"
#include "v8/include/v8-forward.h"

namespace blink {
class WebString;
class WebURL;
}

namespace extensions {

// Used to log DOM API calls from within WebKit. The events are sent via IPC to
// extensions::ActivityLog for recording and display.
class DOMActivityLogger: public blink::WebDOMActivityLogger {
 public:
  static const int kMainWorldId = 0;
  explicit DOMActivityLogger(const ExtensionId& extension_id);

  DOMActivityLogger(const DOMActivityLogger&) = delete;
  DOMActivityLogger& operator=(const DOMActivityLogger&) = delete;

  ~DOMActivityLogger() override;

  // Check (using the WebKit API) if there is no logger attached to the world
  // corresponding to world_id, and if so, construct a new logger and attach it.
  // world_id = 0 indicates the main world.
  static void AttachToWorld(int32_t world_id, const ExtensionId& extension_id);

 private:
  // blink::WebDOMActivityLogger implementation.
  // Marshals the arguments into an ExtensionHostMsg_DOMAction_Params and sends
  // it over to the browser (via IPC) for appending it to the extension activity
  // log.
  // These methods don't have the override keyword due to the complexities it
  // introduces when changes blink apis.
  void LogGetter(v8::Isolate* isolate,
                 v8::Local<v8::Context> context,
                 const blink::WebString& api_name,
                 const blink::WebURL& url,
                 const blink::WebString& title) override;
  void LogSetter(v8::Isolate* isolate,
                 v8::Local<v8::Context> context,
                 const blink::WebString& api_name,
                 const v8::Local<v8::Value>& new_value,
                 const blink::WebURL& url,
                 const blink::WebString& title) override;
  void LogMethod(v8::Isolate* isolate,
                 v8::Local<v8::Context> context,
                 const blink::WebString& api_name,
                 base::span<const v8::Local<v8::Value>> args,
                 const blink::WebURL& url,
                 const blink::WebString& title) override;
  void LogEvent(blink::WebLocalFrame& frame,
                const blink::WebString& event_name,
                base::span<const blink::WebString> args,
                const blink::WebURL& url,
                const blink::WebString& title) override;

  mojom::RendererHost* GetRendererHost(v8::Local<v8::Context> context);

  // The id of the extension with which this logger is associated.
  ExtensionId extension_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_
