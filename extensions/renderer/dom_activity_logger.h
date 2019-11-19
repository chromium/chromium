// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_
#define EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "extensions/common/dom_action_types.h"
#include "third_party/blink/public/web/web_dom_activity_logger.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

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
  explicit DOMActivityLogger(const std::string& extension_id);
  ~DOMActivityLogger() override;

  // Check (using the WebKit API) if there is no logger attached to the world
  // corresponding to world_id, and if so, construct a new logger and attach it.
  // world_id = 0 indicates the main world.
  static void AttachToWorld(int32_t world_id, const std::string& extension_id);

 private:
  // blink::WebDOMActivityLogger implementation.
  // Marshals the arguments into an ExtensionHostMsg_DOMAction_Params and sends
  // it over to the browser (via IPC) for appending it to the extension activity
  // log.
  // These methods don't have the override keyword due to the complexities it
  // introduces when changes blink apis.
  void LogGetter(const blink::WebString& api_name,
                 const blink::WebURL& url,
                 const blink::WebString& title) override;
  void LogSetter(const blink::WebString& api_name,
                 const v8::Local<v8::Value>& new_value,
                 const blink::WebURL& url,
                 const blink::WebString& title) override;
  virtual void logSetter(const blink::WebString& api_name,
                         const v8::Local<v8::Value>& new_value,
                         const v8::Local<v8::Value>& old_value,
                         const blink::WebURL& url,
                         const blink::WebString& title);
  void LogMethod(const blink::WebString& api_name,
                 int argc,
                 const v8::Local<v8::Value>* argv,
                 const blink::WebURL& url,
                 const blink::WebString& title) override;
  void LogEvent(const blink::WebString& event_name,
                int argc,
                const blink::WebString* argv,
                const blink::WebURL& url,
                const blink::WebString& title) override;

  // Helper function to actually send the message across IPC.
  void SendDomActionMessage(const std::string& api_call,
                            const GURL& url,
                            const base::string16& url_title,
                            DomActionType::Type call_type,
                            std::unique_ptr<base::ListValue> args);

  // The id of the extension with which this logger is associated.
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(DOMActivityLogger);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_
