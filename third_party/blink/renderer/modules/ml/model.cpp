// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "modules/ml/Model.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#include "modules/ml/Compilation.h"

namespace blink {

Model::Model(ml::mojom::blink::ModelPtrInfo info) {
  model_.Bind(std::move(info));
  model_.set_connection_error_handler(
      WTF::Bind(&Model::OnConnectionError, WrapWeakPersistent(this)));
}

Model::~Model() {}

ScriptPromise Model::addOperand(ScriptState* script_state,
                                const OperandOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!model_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Model service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  if (!options.hasType()) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "Operand type is missing."));
    return promise;
  }
  int32_t type = options.type();

  WTF::Vector<uint32_t> dimensions;
  float scale = 0;
  int32_t zeroPoint = 0;

  if (options.hasDimensions()) {
    dimensions = options.dimensions();
  }

  if (options.hasScale()) {
    scale = options.scale();
  }

  if (options.hasZeroPoint()) {
    zeroPoint = options.zeroPoint();
  }

  model_->addOperand(
      type, dimensions, scale, zeroPoint,
      WTF::Bind(&Model::OnResultCode, WrapPersistent(this),
                WrapPersistent(resolver), String("addOperand")));
  return promise;
}

ScriptPromise Model::setOperandValue(ScriptState* script_state,
                                     uint32_t index,
                                     MaybeShared<DOMArrayBufferView> data) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!model_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Model service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  uint32_t length = data.View()->byteLength();
  mojo::ScopedSharedBufferHandle buffer = mojo::SharedBufferHandle::Create(length);
  mojo::ScopedSharedBufferMapping mapping = buffer->Map(length);
  memcpy(static_cast<void*>(mapping.get()), data.View()->BaseAddress(), length);
  model_->setOperandValue(
      index, std::move(buffer), length,
      WTF::Bind(&Model::OnResultCode, WrapPersistent(this),
                WrapPersistent(resolver), String("setOperandValue")));

  return promise;
}

ScriptPromise Model::addOperation(ScriptState* script_state,
                                  int32_t type,
                                  Vector<uint32_t>& inputs,
                                  Vector<uint32_t>& outputs) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!model_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Model service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  model_->addOperation(
      type, inputs, outputs,
      WTF::Bind(&Model::OnResultCode, WrapPersistent(this),
                WrapPersistent(resolver), String("addOperation")));

  return promise;
}

ScriptPromise Model::identifyInputsAndOutputs(ScriptState* script_state,
                                              Vector<uint32_t>& inputs,
                                              Vector<uint32_t>& outputs) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!model_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Model service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  model_->identifyInputsAndOutputs(
      inputs, outputs,
      WTF::Bind(&Model::OnResultCode, WrapPersistent(this),
                WrapPersistent(resolver), String("identifyInputsAndOutputs")));

  return promise;
}

ScriptPromise Model::finish(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!model_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Model service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  model_->finish(
      WTF::Bind(&Model::OnResultCode, WrapPersistent(this),
                WrapPersistent(resolver), String("finish")));

  return promise;
}

ScriptPromise Model::createCompilation(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!model_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Model service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  model_->createCompilation(
      WTF::Bind(&Model::OnCreateCompilation,
                WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void Model::OnCreateCompilation(
    ScriptPromiseResolver* resolver, int32_t result_code,
    ml::mojom::blink::CompilationInitParamsPtr init_params) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result_code == ml::mojom::blink::NO_ERROR) {
    resolver->Resolve(new Compilation(std::move(init_params->compilation)));
  } else {
    String msg("createCompilation fails: ");
    msg.append(String::Number(result_code));
    resolver->Reject(DOMException::Create(
                     kInvalidStateError, msg));
  }
}

void Model::OnResultCode(ScriptPromiseResolver* resolver, const String& operation_name, int32_t result_code) {
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

void Model::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
}

void Model::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(DOMException::Create(kNotSupportedError,
                                         "Model is not implemented."));
  }
  requests_.clear();
  model_.reset();
}

}  // namespace blink
