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

Compilation::Compilation(NavigatorML* navigator_ml) {
  navigator_ml->GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&service_));
  service_.set_connection_error_handler(
      WTF::Bind(&Compilation::OnConnectionError, WrapWeakPersistent(this)));
}

Compilation::~Compilation() {}

void Compilation::setModel(Model* model, ExceptionState& exception_state) {
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
  service_->compile(
      model_->GetModelStruct(),
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
