// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/Execution.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#include "modules/ml/NavigatorML.h"
#include "modules/ml/Compilation.h"
#include "modules/ml/Model.h"

namespace blink {

Execution::Execution(NavigatorML* navigator_ml) {
  navigator_ml->GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(
      WTF::Bind(&Execution::OnConnectionError, WrapWeakPersistent(this)));
}

Execution::~Execution() {}

void Execution::setCompilation(Compilation* compilation, ExceptionState& exception_state) {
  if (compilation->IsFinished()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Compilation is not finished.");
  }
  compilation_ = compilation;
  Model* model = compilation_->GetModel();
  input_views_.resize(model->inputs_.size());
  output_views_.resize(model->outputs_.size());
}

void Execution::setInput(uint32_t index,
                         MaybeShared<DOMArrayBufferView> data,
                         ExceptionState& exception_state) {
  
  if (index > input_views_.size()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "index is invalid.");
  }
  input_views_[index] = data.View();
}

void Execution::setOutput(uint32_t index,
                          MaybeShared<DOMArrayBufferView> data,
                          ExceptionState& exception_state) {
  if (index > output_views_.size()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "index is invalid.");
  }
  output_views_[index] = data.View();
}

ScriptPromise Execution::startCompute(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (!service_) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError, "Neural Network service unavailable."));
    return promise;
  }
  requests_.insert(resolver);

  ml::mojom::blink::ComputeRequestPtr compute_request =
      ml::mojom::blink::ComputeRequest::New();
  
  uint32_t total_byte_length = 0;
  for (size_t i = 0; i < input_views_.size(); ++i) {
    DOMArrayBufferView* view = input_views_[i];
    if (view == nullptr) {
      resolver->Reject(DOMException::Create(
          kInvalidStateError, "Input is not set."));
      return promise;
    }
    total_byte_length += view->byteLength();
  }
  for (size_t i = 0; i < output_views_.size(); ++i) {
    DOMArrayBufferView* view = output_views_[i];
    if (view == nullptr) {
      resolver->Reject(DOMException::Create(
          kInvalidStateError, "Output is not set."));
      return promise;
    }
    total_byte_length += view->byteLength();
  }

  compute_request->buffer =
      mojo::SharedBufferHandle::Create(total_byte_length);
  mojo::ScopedSharedBufferMapping mapping =
      compute_request->buffer->Map(total_byte_length);

  uint32_t offset = 0;
  for (size_t i = 0; i < input_views_.size(); ++i) {
    DOMArrayBufferView* view = input_views_[i];
    uint32_t length = view->byteLength();
    ml::mojom::blink::BufferInfoPtr buffer_info =
        ml::mojom::blink::BufferInfo::New(offset, length);
    compute_request->inputs.push_back(std::move(buffer_info));
    uint8_t* base = static_cast<uint8_t*>(mapping.get()) + offset;
    memcpy(static_cast<void*>(base), view->BaseAddress(), length);
    offset += length;
  }
  for (size_t i = 0; i < output_views_.size(); ++i) {
    DOMArrayBufferView* view = output_views_[i];
    uint32_t length = view->byteLength();
    ml::mojom::blink::BufferInfoPtr buffer_info =
        ml::mojom::blink::BufferInfo::New(offset, length);
    compute_request->outputs.push_back(std::move(buffer_info));
    offset += length;
  }

  service_->compute(
      compilation_->GetID(),
      std::move(compute_request),
      WTF::Bind(&Execution::OnComputeDone, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

void Execution::OnComputeDone(ScriptPromiseResolver* resolver, int32_t result) {
  DCHECK(requests_.Contains(resolver));
  requests_.erase(resolver);

  if (result >= 0) {
    resolver->Resolve();
  } else {
    resolver->Reject(DOMException::Create(
                     kInvalidStateError, "Execution fails."));
  }
}

void Execution::Trace(blink::Visitor* visitor) {
  visitor->Trace(requests_);
  visitor->Trace(input_views_);
  visitor->Trace(output_views_);
  visitor->Trace(compilation_);
  ScriptWrappable::Trace(visitor);
}

void Execution::OnConnectionError() {
  for (const auto& request : requests_) {
    request->Reject(DOMException::Create(kNotSupportedError,
                                         "Execution is not implemented."));
  }
  requests_.clear();
  service_.reset();
}

}  // namespace blink
