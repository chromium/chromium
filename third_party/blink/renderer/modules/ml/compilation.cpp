// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Compilation.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#include "modules/ml/Execution.h"

namespace blink {

Compilation::Compilation(ml::mojom::blink::CompilationPtrInfo info) {
  compilation_.Bind(std::move(info));
  compilation_.set_connection_error_handler(
      WTF::Bind(&Compilation::OnConnectionError, WrapWeakPersistent(this)));
}

Compilation::~Compilation() = default;

ScriptPromise Compilation::setPreference(ScriptState* script_state, int32_t preference) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!compilation_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Compilation service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  compilation_->setPreference(
      preference,
      WTF::Bind(&Compilation::OnResultCode, WrapPersistent(this),
                WrapPersistent(resolver), String("finish")));
  return promise;
}

ScriptPromise Compilation::finish(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!compilation_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Compilation service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  compilation_->finish(
      WTF::Bind(&Compilation::OnResultCode, WrapPersistent(this),
                WrapPersistent(resolver), String("finish")));
  return promise;
}

ScriptPromise Compilation::createExecution(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!compilation_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Compilation service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  compilation_->createExecution(
      WTF::Bind(&Compilation::OnCreateExecution,
                WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void Compilation::OnCreateExecution(
    ScriptPromiseResolver* resolver, int32_t result_code,
    ml::mojom::blink::ExecutionInitParamsPtr init_params) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NO_ERROR) {
    resolver->Resolve(new Execution(std::move(init_params)));
  } else {
    String msg("createExecution fails: ");
    msg.append(String::Number(result_code));
    resolver->Reject(DOMException::Create(
                     kInvalidStateError, msg));
  }
}

void Compilation::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
}

void Compilation::OnResultCode(ScriptPromiseResolver* resolver,
                               const String& operation_name,
                               int32_t result_code) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NO_ERROR) {
    resolver->Resolve(result_code);
  } else {
    String msg(operation_name);
    msg.append("fails: ");
    msg.append(String::Number(result_code));
    resolver->Reject(DOMException::Create(
                     kInvalidStateError, msg));
  }
}

void Compilation::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(DOMException::Create(kNotSupportedError,
                                         "Compilation is not implemented."));
  }
  requests_.clear();
  compilation_.reset();
}

}  // namespace blink
