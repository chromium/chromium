// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_
#define EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {

// Used to log extension API calls and events that are implemented with custom
// bindings.The actions are sent via IPC to extensions::ActivityLog for
// recording and display.
class APIActivityLogger : public ObjectBackedNativeHandler {
 public:
  APIActivityLogger(IPCMessageSender* ipc_sender, ScriptContext* context);

  APIActivityLogger(const APIActivityLogger&) = delete;
  APIActivityLogger& operator=(const APIActivityLogger&) = delete;

  ~APIActivityLogger() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

  // Returns true if logging is enabled.
  static bool IsLoggingEnabled();

  // Notifies the browser that an API method has been called, if and only if
  // activity logging is enabled.
  static void LogAPICall(IPCMessageSender* ipc_sender,
                         v8::Local<v8::Context> context,
                         const std::string& call_name,
                         const std::vector<v8::Local<v8::Value>>& arguments);

  // Notifies the browser that an API event has been dispatched, if and only if
  // activity logging is enabled.
  static void LogEvent(IPCMessageSender* ipc_sender,
                       ScriptContext* script_context,
                       const std::string& event_name,
                       base::Value::List arguments);

  static void set_log_for_testing(bool log);

 private:
  // This is ultimately invoked in bindings.js with JavaScript arguments.
  //    arg0 - extension ID as a string
  //    arg1 - API method/Event name as a string
  //    arg2 - arguments to the API call/event
  //    arg3 - any extra logging info as a string (optional)
  // TODO(devlin): Does arg3 ever exist?
  void LogForJS(const IPCMessageSender::ActivityLogCallType call_type,
                const v8::FunctionCallbackInfo<v8::Value>& args);

  // Common implementation method for sending a logging IPC.
  static void LogInternal(IPCMessageSender* ipc_sender,
                          const IPCMessageSender::ActivityLogCallType call_type,
                          const std::string& extension_id,
                          const std::string& call_name,
                          base::Value::List arguments,
                          const std::string& extra);

  // Not owned by |this|.
  // This is owned by NativeExtensionBindingsSystem.
  //
  // Valid to use so long as there's a valid ScriptContext associated with the
  // call-site.
  IPCMessageSender* ipc_sender_ = nullptr;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_ACTIVITY_LOGGER_H_
