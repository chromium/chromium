// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_WEB_REQUEST_EVENT_HANDLING_TRACKER_H_
#define EXTENSIONS_RENDERER_API_WEB_REQUEST_EVENT_HANDLING_TRACKER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class IPCMessageSender;
class ScriptContext;

namespace binding {
class ContextInvalidationListener;
}  // namespace binding

// Tracks which ScriptContexts are handling a blocking webRequest event
// dispatch from the browser (per-context webRequest event dispatch). When the
// last one finishes, sends a single `WebRequestHost.EventHandlingDone` mojo
// call back with no payload: the JS reports each listener's response
// to the browser separately, through "webRequestInternal.eventHandled".
//
// The NativeExtensionBindingsSystem owns one instance per thread (main thread
// and worker threads).
class WebRequestEventHandlingTracker {
 public:
  // Identifies one in-flight dispatch. The browser blocks the request until the
  // signal arrives.
  struct DispatchInfo {
    // The dispatch target's extension; null for non-extension webview
    // embedders.
    std::optional<ExtensionId> extension_id;
    std::string event_name;
    uint64_t request_id = 0;
    int web_view_instance_id = 0;

    auto operator<=>(const DispatchInfo&) const = default;
  };

  explicit WebRequestEventHandlingTracker(IPCMessageSender* ipc_message_sender);
  WebRequestEventHandlingTracker(const WebRequestEventHandlingTracker&) =
      delete;
  WebRequestEventHandlingTracker& operator=(
      const WebRequestEventHandlingTracker&) = delete;
  ~WebRequestEventHandlingTracker();

  // Returns the dispatch info if the dispatch is blocking (the browser awaits
  // a completion signal for the event), or nullopt if not.
  static std::optional<DispatchInfo> GetBlockingDispatchInfo(
      std::optional<ExtensionId> extension_id,
      const std::string& event_name,
      const base::ListValue& event_args);

  // Records that the completion signal must wait for a report from `context`.
  // Call before the event runs in that context.
  void ExpectReportFrom(ScriptContext& context, const DispatchInfo& info);

  // Records that the dispatch notified every receiving context on this thread.
  // After this, the event dispatch waits only for the reports from the
  // contexts. If no report remains to wait for, this call sends the completion
  // signal immediately.
  void OnAllListenersNotified(const DispatchInfo& info);

  // Records that `context` finished resolving its listeners for the dispatch.
  // May send the completion signal.
  void OnContextReported(ScriptContext& context, const DispatchInfo& info);

 private:
  struct PendingDispatch {
    // Contexts that have not reported yet.
    std::set<raw_ptr<ScriptContext>> pending_contexts;
    bool all_listeners_notified = false;
  };
  using PendingDispatchMap = std::map<DispatchInfo, PendingDispatch>;

  // Drops `context` from every dispatch on its invalidation. May send the
  // completion signal.
  void OnContextInvalidated(ScriptContext* context);

  // Sends the completion signal and erases the entry once every listener was
  // notified and no context is pending. Invalidates `it`.
  void MaybeFinalize(PendingDispatchMap::iterator it);

  PendingDispatchMap pending_dispatches_;

  // Watches each context in `pending_dispatches_` for invalidation. An entry
  // stays until its context is invalidated.
  std::map<raw_ptr<ScriptContext>,
           std::unique_ptr<binding::ContextInvalidationListener>>
      invalidation_listeners_;

  const raw_ptr<IPCMessageSender> ipc_message_sender_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_WEB_REQUEST_EVENT_HANDLING_TRACKER_H_
