// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_WORKLET_GLOBAL_SCOPE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class ExceptionState;
class TaskDefinition;

class TaskWorkletGlobalScope : public WorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  TaskWorkletGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                         WorkerThread*);
  ~TaskWorkletGlobalScope() override = default;
  void Trace(blink::Visitor*) override;

  void registerTask(const String& name,
                    const ScriptValue& constructor_value,
                    ExceptionState&);
  v8::Local<v8::Value> GetInstanceForName(const String&, v8::Isolate*);
  v8::Local<v8::Function> GetProcessFunctionForName(const String&,
                                                    v8::Isolate*);

 private:
  HeapHashMap<String, TraceWrapperMember<TaskDefinition>> task_definitions_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_TASK_WORKLET_GLOBAL_SCOPE_H_
