// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_ANCESTRY_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_ANCESTRY_TRACKER_H_

#include <stdint.h>

#include <optional>

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/ad_tracker/ad_script_identifier.h"
#include "third_party/blink/renderer/core/ad_tracker/monkey_patchable_api.h"
#include "third_party/blink/renderer/core/ad_tracker/script_initiation_monitor.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-inspector.h"
#include "v8/include/v8.h"

namespace probe {
class AsyncTaskContext;
}

namespace blink {
class ExecutionContext;
class LazyStackTrace;
class LocalFrame;

// ScriptAncestryTracker provides the common functionality for classes (such as
// AdTracker) that need to keep track of a subset of "marked" scripts and the
// scripts that were later loaded by those marked scripts, transitively. It's
// designed to operate on a local root frame and its local descendants. In
// addition, it provides the useful `IsMarkedScriptInStack` function which
// can be called to determine if a marked script is in the current V8 sync+async
// stack.
//
// Classes that inherit from this, such as AdTracker, should:
// - Figure out how to identify a "root set" of scripts to mark. E.g., the
//   AdTracker checks if the SubresourceFilter has tagged a script resource
//   request or if the execution context is an ad context. It uses
//   `CalculateIfAdSubresource` to learn about SubresourceFilter matches.  An
//   extension tracker would look for the extension schema in the script url
//   in `OnScriptRegistered`.
// - Override `OnScriptRegistered`, which includes the script id of the compiled
//   script as well as any marked script ids in the stack during creation. For
//   script ids that you want to 'mark', keep them in a local datastructure.
// - Implement `IsMarkedScript` which should just be a lookup in your local
//   set of known marked script ids.
class CORE_EXPORT ScriptAncestryTracker
    : public GarbageCollected<ScriptAncestryTracker>,
      public ScriptInitiationMonitor::Observer {
 public:
  // Stack scans of `kBottomOnly` will only scan the bottom frame of the sync
  // stack and also include async frames. `kTopOnly` will scan the top
  // frame, and fall back on the async stack if there is no top frame (e.g.,
  // executing a continuation in blink).
  enum class StackType { kBottomOnly, kTopOnly };

  using MonkeyPatchableApi = ::blink::MonkeyPatchableApi;

  struct ScriptMetadata {
    v8_inspector::V8DebuggerId context_id;
    V8ScriptId marked_script_id;
    String url;
  };

  ScriptAncestryTracker(LocalFrame*, ScriptInitiationMonitor*);
  ScriptAncestryTracker(const ScriptAncestryTracker&) = delete;
  ScriptAncestryTracker& operator=(const ScriptAncestryTracker&) = delete;
  virtual ~ScriptAncestryTracker();

  // ScriptInitiationMonitor::Observer overrides:
  void WillExecuteScript(ExecutionContext& execution_context,
                         v8::Local<v8::Context> v8_context,
                         V8ScriptId script_id,
                         const String& script_url,
                         LazyStackTrace& stack_trace) override;
  void DidExecuteScript(V8ScriptId script_id) override;
  void DidRegisterDynamicScript(v8::Local<v8::Context> v8_context,
                                V8ScriptId script_id,
                                LazyStackTrace& stack_trace) override;
  void WillCallFunction(ExecutionContext& execution_context,
                        V8ScriptId script_id,
                        bool is_nested,
                        LazyStackTrace& stack_trace) override;
  void DidCallFunction(V8ScriptId script_id, bool is_nested) override;
  void DidCreateAsyncTask(probe::AsyncTaskContext* task_context,
                          LazyStackTrace& stack_trace) override;
  void DidStartAsyncTask(probe::AsyncTaskContext* task_context) override;
  void DidFinishAsyncTask(probe::AsyncTaskContext* task_context) override;

  // Returns true if the script at the top of isolate's
  // stack is a marked script (or bottom, depending on `StackType`). `kTopOnly`
  // is generally best since it's directly triggering blink native code, but you
  // may want to call `kBottomOnly` if you truly only care about that frame.
  //
  // When `ignore_monkey_patch` is specified, a heuristic is enabled to mitigate
  // inaccurate stack tagging caused by API monkey patching (i.e., the immediate
  // caller is a proxy for the real caller). This handles two distinct
  // scenarios:
  // 1. Marked Script Monkey Patch (Mitigating False Positives):
  //    If the script at the top of the stack is marked and the API was
  //    invoked by a non-marked script, this check will be ignored for the first
  //    call to the specified API within the current synchronous task.
  // 2. Unmarked Monkey Patch (Mitigating False Negatives):
  //    If the script at the top of the stack is an unmarked script and the API
  //    was invoked by a marked script, the stack is classified as marked.
  //
  // Note: This function is not idempotent when `ignore_monkey_patch` is used,
  // as it tracks the first call to an API within a synchronous task.
  //
  // Output Parameters:
  // - `out_script`: if non-null and there is a marked script in the
  //   stack, this will be populated with the marked script's id.
  bool IsMarkedScriptInStack(
      StackType stack_type,
      LazyStackTrace& stack_trace,
      std::optional<V8ScriptId>* out_script = nullptr,
      MonkeyPatchableApi ignore_monkey_patch = MonkeyPatchableApi::kNone);

  virtual void Shutdown();
  void Trace(Visitor*) const override;

 protected:
  // Called by the base class to check if a specific script ID is a marked
  // script belonging to the subclass's tracking domain (e.g., ad scripts,
  // extension scripts).
  virtual bool IsMarkedScript(V8ScriptId) const = 0;

  // Called by the base class whenever a new script has been compiled, resolved,
  // and registered in the ancestry tracking graph.
  //
  // Subclasses MUST override this method to perform domain-specific
  // classification. For example, they can check if the script matches a
  // specific URL pattern, is run within an ad frame context, or belongs to an
  // isolated world origin. If the script is classified as belonging to the
  // subclass's domain, the subclass should tag the `script_id` in its private
  // registries to recognize it during subsequent call stack traversal queries.
  //
  // Parameters:
  // - `execution_context`: The execution context (e.g., DOM Window)
  // compiling/running the script (guaranteed to be non-null).
  // - `script_id`: The unique V8 compiler ID of the newly registered script.
  // - `url`: The resolved URL of the script (e.g., external source URL or a
  // synthetic URL generated for inline scripts).
  // - `marked_script_id`: The ID of the marked script that initiated
  // compilation, if any.
  virtual void OnScriptRegistered(
      ExecutionContext& execution_context,
      V8ScriptId script_id,
      const String& url,
      std::optional<V8ScriptId> marked_script_id) = 0;

  // Explicitly registers a script (like inline attribute event handlers) that
  // is compiled dynamically rather than run as a standalone script.
  void RegisterScript(v8::Local<v8::Context> v8_context,
                      V8ScriptId script_id,
                      std::optional<V8ScriptId> marked_script_id);

  // Retrieves generic metadata (like parent script relationship and resolved
  // URL) for a given V8ScriptId.
  const ScriptMetadata* GetScriptMetadata(V8ScriptId script_id) const;

 private:
  // Helper for the `ignore_monkey_patch` heuristic. Returns true if the API is
  // called from a non-ad script through an ad script's monkey patch, and this
  // is the first time this API has been called this way within the current
  // synchronous task. If it returns true, the call should be ignored for ad
  // tracking purposes. This method is not const because it modifies
  // `ad_monkey_patch_calls_in_scope_`.
  //
  // Precondition: The script at the top of the stack is a known ad script.
  bool IsFirstCallOfApiFromNonAttributedScript(v8::Isolate* isolate,
                                               MonkeyPatchableApi api,
                                               LazyStackTrace& stack_trace);

  // Helper for `IsFirstCallOfApiFromNonAttributedScript` that performs the
  // stack analysis. It returns true if the call stack indicates that a
  // non-marked script called the monkey patched `api`.
  bool WasApiCalledByNonAttributedScript(v8::Isolate* isolate,
                                         MonkeyPatchableApi api,
                                         LazyStackTrace& stack_trace) const;

  // Returns true if `api` is a monkeypatched function and matches `function` in
  // the `isolate`'s current context.
  // WARNING: This function executes js and can therefore modify the members of
  // this class. Consider all iterators obtained before calling
  // IsFunctionAMonkeyPatch to be invalid.
  // TODO(jkarlin): This function really wants a context, not an isolate.
  bool IsFunctionAMonkeyPatch(v8::Isolate* isolate,
                              const v8::Local<v8::Function>& function,
                              MonkeyPatchableApi api) const;

  // The local root frame of the frame tree monitored by this tracker.
  Member<LocalFrame> local_root_;

  // The central script initiation monitor dispatching execution and task
  // context events to this tracker.
  Member<ScriptInitiationMonitor> monitor_;

  // Indicates the bottom-most synchronous script on the current call stack.
  std::optional<V8ScriptId> bottom_most_script_;

  // Simulates the active asynchronous execution stack by pushing and popping
  // initiating marked script IDs as asynchronous/event-driven tasks start and
  // finish.
  Vector<std::optional<V8ScriptId>> async_script_stack_;

  // The permanent registry mapping each compiled V8ScriptId to its resolved URL
  // and marked parent script ID. This represents our compilation graph for
  // marked scripts and their descendants.
  HashMap<V8ScriptId, ScriptMetadata> script_metadata_;

  // Counter tracking the number of active synchronous script/task executions
  // on the call stack.
  int running_sync_tasks_ = 0;

  // Tracks active monkey-patched API calls in the current task scope to ensure
  // the ignore-monkey-patch heuristic only applies once per task.
  HashSet<MonkeyPatchableApi> monkey_patch_calls_in_scope_;

  friend class AdTrackerTest;
  friend class ScriptAncestryTrackerTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_ANCESTRY_TRACKER_H_
