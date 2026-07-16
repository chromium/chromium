// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_INITIATION_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_INITIATION_MONITOR_H_

#include "third_party/blink/renderer/core/ad_tracker/lazy_stack_trace.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/ad_tagging_utils.h"

namespace v8 {
class Context;
template <class T>
class Local;
}  // namespace v8

namespace blink {

class Document;
class DocumentLoader;
class LocalFrame;
class ExecutionContext;
class KURL;
enum class ResourceType : uint8_t;

class ResourceRequestHead;
struct FetchInitiatorInfo;

namespace probe {
class ExecuteScript;
class CallFunction;
class AsyncTaskContext;
class PrepareRequest;
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
                                   v8::Local<v8::Context> v8_context,
                                   V8ScriptId script_id,
                                   const String& script_url,
                                   LazyStackTrace& stack_trace) = 0;
    // Called after a top-level script execution completes.
    virtual void DidExecuteScript(V8ScriptId script_id) = 0;

    // Called when a dynamic script (e.g., inline event handler) is registered.
    virtual void DidRegisterDynamicScript(v8::Local<v8::Context> v8_context,
                                          V8ScriptId script_id,
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

    // Called when a subresource request is prepared. This is the point that
    // observers can detect new scripts being fetched, and check the stack
    // (sync or async) to see if it needs to be tracked.
    virtual void WillPrepareRequest(
        Document* document,
        const ResourceRequestHead& request,
        std::optional<KURL> alias_url,
        ResourceType resource_type,
        const FetchInitiatorInfo& initiator_info,
        std::optional<AdProvenance> known_ad_provenance,
        bool scan_javascript_stack,
        LazyStackTrace& stack_trace) = 0;
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

  // Probe hooks called by core_probes. Do not call directly.
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);
  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);
  void DidCreateAsyncTask(probe::AsyncTaskContext*);
  void DidStartAsyncTask(probe::AsyncTaskContext*);
  void DidFinishAsyncTask(probe::AsyncTaskContext*);

  // Manually invoked by the loader pipeline during subresource request
  // preparation to notify observers.
  void PrepareRequest(DocumentLoader*,
                      const ResourceRequestHead&,
                      std::optional<KURL> alias_url,
                      ResourceType,
                      const FetchInitiatorInfo&,
                      std::optional<AdProvenance>,
                      bool scan_javascript_stack);

  void DidRegisterDynamicScript(v8::Local<v8::Context> v8_context,
                                V8ScriptId script_id);

  void Trace(Visitor*) const;

 private:
  Member<LocalFrame> local_root_;
  HeapHashSet<WeakMember<Observer>> observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_SCRIPT_INITIATION_MONITOR_H_
