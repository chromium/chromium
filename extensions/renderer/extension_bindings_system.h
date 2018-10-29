// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_EXTENSION_BINDINGS_SYSTEM_H_

#include <string>

#include "base/time/time.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"

namespace base {
class ListValue;
}

namespace extensions {
class IPCMessageSender;
class RendererMessagingService;
class RequestSender;
class ScriptContext;
struct EventFilteringInfo;

// The class responsible for creating extension bindings in different contexts,
// as well as dispatching requests and handling responses, and dispatching
// events to listeners.
// This is designed to be used on a single thread, but should be safe to use on
// threads other than the main thread (so that worker threads can have extension
// bindings).
class ExtensionBindingsSystem {
 public:
  virtual ~ExtensionBindingsSystem() {}

  // Called when a new ScriptContext is created.
  virtual void DidCreateScriptContext(ScriptContext* context) = 0;

  // Called when a ScriptContext is about to be released.
  virtual void WillReleaseScriptContext(ScriptContext* context) = 0;

  // Updates the bindings for a given |context|. This happens at initialization,
  // but also when e.g. an extension gets updated permissions.
  virtual void UpdateBindingsForContext(ScriptContext* context) = 0;

  // Dispatches an event with the given |name|, |event_args|, and
  // |filtering_info| in the given |context|.
  virtual void DispatchEventInContext(const std::string& event_name,
                                      const base::ListValue* event_args,
                                      const EventFilteringInfo* filtering_info,
                                      ScriptContext* context) = 0;

  // Returns true if there is a listener for the given |event_name| in the
  // associated |context|.
  virtual bool HasEventListenerInContext(const std::string& event_name,
                                         ScriptContext* context) = 0;

  // Handles the response associated with the given |request_id|.
  virtual void HandleResponse(int request_id,
                              bool success,
                              const base::ListValue& response,
                              const std::string& error) = 0;

  // Returns the associated IPC message sender.
  virtual IPCMessageSender* GetIPCMessageSender() = 0;

  // Returns the associated RequestSender, if any.
  // TODO(devlin): Factor this out.
  virtual RequestSender* GetRequestSender() = 0;

  // Returns the associated RendererMessagingService.
  virtual RendererMessagingService* GetMessagingService() = 0;

  // Called when an extension is removed.
  virtual void OnExtensionRemoved(const ExtensionId& id) {}

  // Called when an extension's permissions are updated.
  virtual void OnExtensionPermissionsUpdated(const ExtensionId& id) {}

  // Returns true if any portion of the runtime API is available to the given
  // |context|. This is different than just checking features because runtime's
  // availability depends on the installed extensions and the active URL (in the
  // case of extensions communicating with external websites).
  static bool IsRuntimeAvailableToContext(ScriptContext* context);

  // Logs the amount of time taken to update the bindings for a given context
  // (i.e., UpdateBindingsForContext()).
  static void LogUpdateBindingsForContextTime(Feature::Context context_type,
                                              base::TimeDelta elapsed);

  // The APIs that could potentially be available to webpage-like contexts.
  // This is the list of possible features; most web pages will not have access
  // to these APIs.
  // Note: `runtime` is not included here, since it's handled specially above.
  // Note: We specify the size of the array to allow for its use in for loops
  // without needing to expose a separate "kNumWebAvailableFeatures".
  static const char* const kWebAvailableFeatures[2];
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_BINDINGS_SYSTEM_H_
