// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_INITIATION_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_INITIATION_MONITOR_H_

#include "base/check_deref.h"
#include "base/check_op.h"
#include "third_party/blink/renderer/core/ad_tracker/lazy_stack_trace.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/ad_tagging_utils.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LocalFrame;
class ExecutionContext;

namespace probe {
class ExecuteScript;
class CallFunction;
class AsyncTaskContext;
}  // namespace probe

// Centralized manager for monitoring script execution, async task boundaries,
// and network request preparation across a frame. It listens to core engine
// events and broadcasts them to registered Observers with a shared, lazy stack
// trace.
//
// Scope: A single instance is created for the local root frame and shared by
// all its local descendants. It monitors events across this entire local frame
// tree.
//
// This decouples specialized script-tracking logic from the core probe/loader
// infrastructure.
class CORE_EXPORT ScriptInitiationMonitor
    : public GarbageCollected<ScriptInitiationMonitor> {
 public:
  // Observers must implement this interface to listen to script events.
  class CORE_EXPORT Observer : public GarbageCollectedMixin {
   public:
    // Called before the engine executes a top-level script (e.g.,
    // parser-inserted or external script resource).
    virtual void WillExecuteScript(ExecutionContext& execution_context,
                                   V8ScriptId script_id,
                                   const String& script_url,
                                   LazyStackTrace& stack_trace) = 0;
    // Called after a top-level script execution completes.
    virtual void DidExecuteScript(V8ScriptId script_id) = 0;

    // Called when a dynamic script (e.g., inline event handler) is registered.
    virtual void DidRegisterDynamicScript(V8ScriptId script_id,
                                          LazyStackTrace& stack_trace) = 0;

    // Called before a JS function is called (e.g., event listener callbacks).
    virtual void WillCallFunction(ExecutionContext& execution_context,
                                  V8ScriptId script_id,
                                  bool is_nested,
                                  LazyStackTrace& stack_trace) = 0;
    // Called after a JS function call completes.
    virtual void DidCallFunction(V8ScriptId script_id, bool is_nested) = 0;

    // Async task lifecycle hooks to track provenance across async boundaries.
    virtual void DidCreateAsyncTask(probe::AsyncTaskContext* task_context,
                                    LazyStackTrace& stack_trace) = 0;
    virtual void DidStartAsyncTask(probe::AsyncTaskContext* task_context) = 0;
    virtual void DidFinishAsyncTask(probe::AsyncTaskContext* task_context) = 0;

    // Frame lifecycle hooks to track provenance of child frames created by
    // marked scripts.
    virtual void DidCreateFrame(LocalFrame* frame,
                                LazyStackTrace& stack_trace) {}
    virtual void DidSwapFrame(LocalFrame* old_frame, LocalFrame* new_frame) {}
  };

  // Helper to retrieve the monitor from the current ExecutionContext.
  static ScriptInitiationMonitor* FromExecutionContext(ExecutionContext*);

  explicit ScriptInitiationMonitor(LocalFrame*);
  ScriptInitiationMonitor(const ScriptInitiationMonitor&) = delete;
  ScriptInitiationMonitor& operator=(const ScriptInitiationMonitor&) = delete;
  virtual ~ScriptInitiationMonitor();

  // Called during frame detachment to clean up observers.
  void Shutdown();

  void AddObserver(Observer*);
  void RemoveObserver(Observer*);

  // Notifies observers when a LocalFrame has been created in the frame tree.
  void DidCreateLocalFrame(LocalFrame* frame);

  // Notifies observers when a LocalFrame has been swapped in place of an
  // existing frame.
  void DidSwapLocalFrame(LocalFrame* old_frame, LocalFrame* new_frame);

  // Probe hooks called by core_probes. Do not call directly.
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);
  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);
  void DidCreateAsyncTask(probe::AsyncTaskContext*);
  void DidStartAsyncTask(probe::AsyncTaskContext*);
  void DidFinishAsyncTask(probe::AsyncTaskContext*);

  void DidRegisterDynamicScript(V8ScriptId script_id);

  // Returns the V8 isolate for this monitor's local root, falling back to the
  // current thread isolate if the window is unavailable.
  v8::Isolate* GetIsolate() const;

  // Stack-allocated RAII scope used to tag synchronous script
  // compilation/execution initiated by extension injectors.
  class ScopedInjectedExtensionScriptExecution {
    STACK_ALLOCATED();

   public:
    ScopedInjectedExtensionScriptExecution(ScriptInitiationMonitor* monitor,
                                           const String& script_injector_id)
        : monitor_(CHECK_DEREF(monitor)) {
      CHECK(!script_injector_id.empty());
      monitor_.EnterInjectedExtensionScriptExecution(script_injector_id);
    }
    ScopedInjectedExtensionScriptExecution(
        const ScopedInjectedExtensionScriptExecution&) = delete;
    ScopedInjectedExtensionScriptExecution& operator=(
        const ScopedInjectedExtensionScriptExecution&) = delete;
    ~ScopedInjectedExtensionScriptExecution() {
      monitor_.ExitInjectedExtensionScriptExecution();
    }

   private:
    // Reference is safe because this object is STACK_ALLOCATED() and will
    // not outlive the stack frame of the executing script.
    ScriptInitiationMonitor& monitor_;
  };

  // Returns the extension/injector ID for the currently executing injected
  // script, or an empty String if none.
  String CurrentScriptInjectorId() const {
    return injected_script_injector_ids_.empty()
               ? String()
               : injected_script_injector_ids_.back();
  }

  void Trace(Visitor*) const;

 private:
  void EnterInjectedExtensionScriptExecution(const String& script_injector_id) {
    injected_script_injector_ids_.emplace_back(script_injector_id);
  }
  void ExitInjectedExtensionScriptExecution() {
    CHECK(!injected_script_injector_ids_.empty());
    injected_script_injector_ids_.pop_back();
  }

 private:
  Member<LocalFrame> local_root_;
  HeapHashSet<WeakMember<Observer>> observers_;
  Vector<String> injected_script_injector_ids_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_INITIATION_MONITOR_H_
