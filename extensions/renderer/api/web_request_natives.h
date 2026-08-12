// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_WEB_REQUEST_NATIVES_H_
#define EXTENSIONS_RENDERER_API_WEB_REQUEST_NATIVES_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "extensions/common/api/web_request/web_request_filter.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;

// Custom bindings for the webRequest API.
class WebRequestNatives : public ObjectBackedNativeHandler {
 public:
  WebRequestNatives(ScriptContext* context,
                    NativeExtensionBindingsSystem* bindings_system);

  WebRequestNatives(const WebRequestNatives&) = delete;
  WebRequestNatives& operator=(const WebRequestNatives&) = delete;
  ~WebRequestNatives() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  struct TrackedListener {
    std::string event_name;
    WebRequestParsedFilter filter;
    int web_view_instance_id = 0;
    bool is_blocking = false;
    bool is_async_blocking = false;
  };

  void AllowAsyncResponsesForAllEvents(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void IsPerContextEventDispatchEnabled(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // TrackListener(eventName, id, filter, webViewInstanceId, isBlocking,
  // isAsyncBlocking): records a per-context listener's matching data under
  // `id`, parsing its `webRequest.RequestFilter`. A listener whose filter
  // fails to parse is not tracked.
  void TrackListener(const v8::FunctionCallbackInfo<v8::Value>& args);

  // UntrackListener(id): drops the recorded matching data for a removed
  // listener. No-op for untracked IDs.
  void UntrackListener(const v8::FunctionCallbackInfo<v8::Value>& args);

  // GetMatchingListeners(eventName, url, type, tabId, windowId, instanceId,
  // wantsResponse): returns the IDs of `eventName`'s tracked listeners whose
  // `webRequest.RequestFilter` matches the request and whose webview instance
  // ID equals `instanceId` (0 for non-webview listeners). Blocking listeners
  // are excluded from dispatches that do not want a response.
  // Mirrors the filter checks of the browser's
  // `WebRequestEventRouter::ListenerMatchesRequest()`.
  void GetMatchingListeners(const v8::FunctionCallbackInfo<v8::Value>& args);

  // ReportEventHandlingDone(eventName, requestId, webViewInstanceId): reports
  // that this context finished handling a blocking webRequest event. Carries
  // no responses; those travel through webRequestInternal.eventHandled.
  void ReportEventHandlingDone(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Tracked per-context listeners, keyed by the JS-assigned listener ID,
  // which is unique within the context across all events.
  base::flat_map<int, TrackedListener> tracked_listeners_;

  // The bindings system of the context's thread.
  const raw_ptr<NativeExtensionBindingsSystem> bindings_system_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_WEB_REQUEST_NATIVES_H_
