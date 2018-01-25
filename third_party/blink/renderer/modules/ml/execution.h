// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Execution_h
#define Execution_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "services/ml/public/interfaces/neuralnetwork.mojom-blink.h"

namespace blink {

class Compilation;
class ExceptionState;
class NavigatorML;

class Execution final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Execution(NavigatorML*);
  ~Execution() override;

  void setCompilation(Compilation*, ExceptionState&);
  void setInput(uint32_t, MaybeShared<DOMArrayBufferView>, ExceptionState&);
  void setOutput(uint32_t, MaybeShared<DOMArrayBufferView>, ExceptionState&);
  ScriptPromise startCompute(ScriptState*);

  void Trace(blink::Visitor*);
 private:
  void OnComputeDone(ScriptPromiseResolver*, int32_t);
  void OnConnectionError();

  ml::mojom::blink::NeuralNetworkPtr service_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;

  Member<Compilation> compilation_;
  HeapVector<Member<DOMArrayBufferView>> input_views_;
  HeapVector<Member<DOMArrayBufferView>> output_views_;
};

}  // namespace blink

#endif  // Execution_h