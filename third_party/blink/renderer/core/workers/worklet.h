// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;

// This is the base implementation of Worklet interface defined in the spec:
// https://drafts.css-houdini.org/worklets/#worklet
// Although some worklets run off the main thread, this must be created and
// destroyed on the main thread.
class CORE_EXPORT Worklet : public ScriptWrappable,
                            public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Worklet);
  USING_PRE_FINALIZER(Worklet, Dispose);

 public:
  ~Worklet() override;

  void Dispose();

  // Worklet.idl
  // addModule() imports ES6 module scripts.
  ScriptPromise addModule(ScriptState*,
                          const String& module_url,
                          const WorkletOptions*,
                          ExceptionState&);

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  // Returns true if there is ongoing module loading tasks. BaseAudioContext
  // uses this check to keep itself alive until pending tasks are resolved.
  bool HasPendingTasks() const;

  // Called by WorkletPendingTasks to notify the Worklet.
  void FinishPendingTasks(WorkletPendingTasks*);

  void Trace(blink::Visitor*) override;

 protected:
  explicit Worklet(Document*);

  // Returns one of available global scopes.
  WorkletGlobalScopeProxy* FindAvailableGlobalScope();

  wtf_size_t GetNumberOfGlobalScopes() const { return proxies_.size(); }

  WorkletModuleResponsesMap* ModuleResponsesMap() const {
    return module_responses_map_.Get();
  }

  // "A Worklet has a list of the worklet's WorkletGlobalScopes. Initially this
  // list is empty; it is populated when the user agent chooses to create its
  // WorkletGlobalScope."
  // https://drafts.css-houdini.org/worklets/#worklet-section
  HeapVector<Member<WorkletGlobalScopeProxy>> proxies_;

 private:
  virtual void FetchAndInvokeScript(const KURL& module_url_record,
                                    const String& credentials,
                                    WorkletPendingTasks*);

  // Returns true if there are no global scopes or additional global scopes are
  // necessary. CreateGlobalScope() will be called in that case. Each worklet
  // can define how to pool global scopes here.
  virtual bool NeedsToCreateGlobalScope() = 0;
  virtual WorkletGlobalScopeProxy* CreateGlobalScope() = 0;

  // A worklet may or may not have more than one global scope. In the case where
  // there are multiple global scopes, this function MUST be overriden. The
  // default behavior is to return the global scope at index 0, which is for the
  // case where there is only one global scope.
  virtual wtf_size_t SelectGlobalScope();
  // "A Worklet has a module responses map. This is a ordered map of module URLs
  // to values that are a fetch responses. The map's entries are ordered based
  // on their insertion order. Access to this map should be thread-safe."
  // https://drafts.css-houdini.org/worklets/#module-responses-map
  Member<WorkletModuleResponsesMap> module_responses_map_;

  // Keeps track of pending tasks from addModule() call.
  HeapHashSet<Member<WorkletPendingTasks>> pending_tasks_set_;

  DISALLOW_COPY_AND_ASSIGN(Worklet);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKLET_H_
