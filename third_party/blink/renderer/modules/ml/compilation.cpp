// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Compilation.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#include "modules/ml/NavigatorML.h"
#include "modules/ml/Model.h"

namespace blink {

Compilation::Compilation(NavigatorML* navigator_ml) : is_finished_(false) {
  navigator_ml->GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(
      WTF::Bind(&Compilation::OnConnectionError, WrapWeakPersistent(this)));
}

Compilation::~Compilation() {}

void Compilation::setModel(Model* model, ExceptionState& exception_state) {
  if (!model->IsFinished()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Model has not been finished.");
  }
  model_ = model;
}

void Compilation::setPreference(int32_t preference, ExceptionState& exception_state) {
  preference_ = preference;
}

ScriptPromise Compilation::finish(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!service_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Compilation service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  ml::mojom::blink::ModelPtr mojo_model =
      ml::mojom::blink::Model::New();

  mojo_model->inputs = model_->inputs_;
  mojo_model->outputs = model_->outputs_;
  
  for (size_t i = 0; i < model_->operands_.size(); ++i) {
    Operand operand = model_->operands_[i];
    ml::mojom::blink::BufferInfoPtr mojo_buffer_info =
        ml::mojom::blink::BufferInfo::New(0, 0);
    ml::mojom::blink::OperandPtr mojo_operand =
        ml::mojom::blink::Operand::New(
            operand.type,
            operand.dimensions,
            operand.scale,
            operand.zeroPoint,
            std::move(mojo_buffer_info));
    mojo_model->operands.push_back(std::move(mojo_operand));
  }

  for (size_t i = 0; i < model_->operations_.size(); ++i) {
    Operation operation = model_->operations_[i];
    ml::mojom::blink::OperationPtr mojo_operation =
        ml::mojom::blink::Operation::New(
            operation.type,
            operation.inputs,
            operation.outputs);
    mojo_model->operations.push_back(std::move(mojo_operation));
  }

  uint32_t total_byte_length = 0;
  for (size_t i = 0; i < model_->buffer_view_indexes_.size(); ++i) {
    DOMArrayBufferView* view = model_->buffer_views_[i];
    total_byte_length += view->byteLength();
  }

  mojo_model->buffer = mojo::SharedBufferHandle::Create(total_byte_length);
  mojo::ScopedSharedBufferMapping mapping = mojo_model->buffer->Map(total_byte_length);

  uint32_t offset = 0;
  for (size_t i = 0; i < model_->buffer_view_indexes_.size(); ++i) {
    uint32_t index = model_->buffer_view_indexes_[i];
    DOMArrayBufferView* view = model_->buffer_views_[i];
    uint32_t length = view->byteLength();
    const ml::mojom::blink::OperandPtr& operand =
        mojo_model->operands[index];
    operand->bufferInfo->offset = offset;
    operand->bufferInfo->length = length;
    uint8_t* base = static_cast<uint8_t*>(mapping.get()) + offset;
    memcpy(static_cast<void*>(base), view->BaseAddress(), length);
    offset += length;
  }

  service_->compile(
      std::move(mojo_model),
      preference_,
      WTF::Bind(&Compilation::OnCompileDone, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void Compilation::OnCompileDone(ScriptPromiseResolver* resolver, int32_t result) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result >= 0) {
    id_ = result;
    resolver->Resolve();
  } else {
    resolver->Reject(DOMException::Create(
                     kInvalidStateError, "Compilation fails."));
  }
}

void Compilation::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  visitor->Trace(model_);
  ScriptWrappable::Trace(visitor);
}

void Compilation::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(DOMException::Create(kNotSupportedError,
                                         "Compilation is not implemented."));
  }
  requests_.clear();
  service_.reset();
}

}  // namespace blink
