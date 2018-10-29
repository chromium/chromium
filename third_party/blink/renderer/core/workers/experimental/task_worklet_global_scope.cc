// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/experimental/task_worklet_global_scope.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_parser.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

namespace blink {

class TaskDefinition final : public GarbageCollectedFinalized<TaskDefinition> {
 public:
  TaskDefinition(v8::Isolate* isolate,
                 v8::Local<v8::Value> instance,
                 v8::Local<v8::Function> process)
      : instance_(isolate, instance), process_(isolate, process) {}
  ~TaskDefinition() = default;

  v8::Local<v8::Value> InstanceLocal(v8::Isolate* isolate) {
    return instance_.NewLocal(isolate);
  }
  v8::Local<v8::Function> ProcessLocal(v8::Isolate* isolate) {
    return process_.NewLocal(isolate);
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(instance_.Cast<v8::Value>());
    visitor->Trace(process_.Cast<v8::Value>());
  }

 private:
  // This object keeps the object and process function alive.
  // It participates in wrapper tracing as it holds onto V8 wrappers.
  TraceWrapperV8Reference<v8::Value> instance_;
  TraceWrapperV8Reference<v8::Function> process_;
};

TaskWorkletGlobalScope::TaskWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread)
    : WorkletGlobalScope(std::move(creation_params),
                         thread->GetWorkerReportingProxy(),
                         thread) {}

void TaskWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  WorkletGlobalScope::Trace(visitor);
  visitor->Trace(task_definitions_);
}

void TaskWorkletGlobalScope::registerTask(const String& name,
                                          const ScriptValue& constructor_value,
                                          ExceptionState& exception_state) {
  DCHECK(IsContextThread());
  if (task_definitions_.Contains(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Context> context = ScriptController()->GetContext();

  DCHECK(constructor_value.V8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(constructor_value.V8Value());

  v8::Local<v8::Object> prototype;
  if (!V8ObjectParser::ParsePrototype(context, constructor, &prototype,
                                      &exception_state))
    return;

  v8::Local<v8::Function> process;
  if (!V8ObjectParser::ParseFunction(context, prototype, "process", &process,
                                     &exception_state))
    return;

  v8::Local<v8::Value> instance;
  bool did_construct =
      V8ScriptRunner::CallAsConstructor(isolate, constructor, this)
          .ToLocal(&instance);
  if (!did_construct) {
    exception_state.ThrowTypeError("Failed to construct TaskProcessor");
    return;
  }

  TaskDefinition* definition = new TaskDefinition(isolate, instance, process);
  task_definitions_.Set(name, definition);
}

v8::Local<v8::Value> TaskWorkletGlobalScope::GetInstanceForName(
    const String& name,
    v8::Isolate* isolate) {
  TaskDefinition* definition = task_definitions_.at(name);
  return definition ? definition->InstanceLocal(isolate)
                    : v8::Local<v8::Value>();
}

v8::Local<v8::Function> TaskWorkletGlobalScope::GetProcessFunctionForName(
    const String& name,
    v8::Isolate* isolate) {
  TaskDefinition* definition = task_definitions_.at(name);
  return definition ? definition->ProcessLocal(isolate)
                    : v8::Local<v8::Function>();
}

}  // namespace blink
