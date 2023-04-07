// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_operation_definition.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_run_function_for_shared_storage_run_operation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_run_function_for_shared_storage_select_url_operation.h"

namespace blink {

SharedStorageOperationDefinition::SharedStorageOperationDefinition(
    ScriptState* script_state,
    const String& name,
    V8NoArgumentConstructor* constructor,
    v8::Local<v8::Function> v8_run)
    : script_state_(script_state),
      name_(name),
      constructor_(constructor),
      run_function_for_select_url_(
          V8RunFunctionForSharedStorageSelectURLOperation::Create(v8_run)),
      run_function_for_run_(
          V8RunFunctionForSharedStorageRunOperation::Create(v8_run)) {}

SharedStorageOperationDefinition::~SharedStorageOperationDefinition() = default;

void SharedStorageOperationDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(constructor_);
  visitor->Trace(run_function_for_select_url_);
  visitor->Trace(run_function_for_run_);
  visitor->Trace(instance_);
  visitor->Trace(script_state_);
}

TraceWrapperV8Reference<v8::Value>
SharedStorageOperationDefinition::GetInstance() {
  if (did_call_constructor_) {
    return instance_;
  }

  did_call_constructor_ = true;

  CHECK(instance_.IsEmpty());

  ScriptValue instance;
  if (!constructor_->Construct().To(&instance)) {
    return TraceWrapperV8Reference<v8::Value>();
  }

  instance_.Reset(constructor_->GetIsolate(), instance.V8Value());
  return instance_;
}

}  // namespace blink
