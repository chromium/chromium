// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/compilation.h"

#include <utility>

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/ml/public/mojom/constants.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/execution.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

Compilation::Compilation(ml::mojom::blink::CompilationPtrInfo info)
    : is_finished_(false) {
  compilation_.Bind(std::move(info));
  compilation_.set_connection_error_handler(
      WTF::Bind(&Compilation::OnConnectionError, WrapWeakPersistent(this)));
}

Compilation::~Compilation() = default;

void Compilation::setPreference(int32_t preference,
                                ExceptionState& exception_state) {
  if (is_finished_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Compilation is finished.");
    return;
  }

  preference_ = preference;
}

ScriptPromise Compilation::finish(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (is_finished_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                          "Compilation is finished."));
    return promise;
  }
  if (!compilation_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                          "Compilation service unavailable."));
    return promise;
  }

  requests_.insert(resolver);

  compilation_->Finish(
      preference_, WTF::Bind(&Compilation::OnResultCode, WrapPersistent(this),
                             WrapPersistent(resolver), String("finish")));
  is_finished_ = true;
  return promise;
}

ScriptPromise Compilation::createExecution(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!is_finished_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                          "Compilation is not finished."));
    return promise;
  }
  if (!compilation_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                          "Compilation service unavailable."));
    return promise;
  }

  requests_.insert(resolver);

  compilation_->CreateExecution(WTF::Bind(&Compilation::OnCreateExecution,
                                          WrapPersistent(this),
                                          WrapPersistent(resolver)));
  return promise;
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

  if (result_code == ml::mojom::blink::NOT_ERROR) {
    resolver->Resolve(result_code);
  } else {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kInvalidStateError,
        operation_name + "fails: " + String::Number(result_code)));
  }
}

void Compilation::OnCreateExecution(
    ScriptPromiseResolver* resolver,
    int32_t result_code,
    ml::mojom::blink::ExecutionInitParamsPtr init_params) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NOT_ERROR) {
    resolver->Resolve(MakeGarbageCollected<Execution>(std::move(init_params)));
  } else {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kInvalidStateError,
        "createExecution fails: " + String::Number(result_code)));
  }
}

void Compilation::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                         "Compilation is not implemented."));
  }

  requests_.clear();
  compilation_.reset();
}

}  // namespace blink
