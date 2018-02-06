// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Model_h
#define Model_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/wtf/Vector.h"
#include "modules/ml/OperandOptions.h"

#include "services/ml/public/interfaces/model.mojom-blink.h"

namespace blink {

class Model final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Model(ml::mojom::blink::ModelPtrInfo);
  ~Model() override;

  void addOperand(const OperandOptions&, ExceptionState&);
  void setOperandValue(uint32_t, MaybeShared<DOMArrayBufferView>, ExceptionState&);
  void addOperation(int32_t, Vector<uint32_t>&, Vector<uint32_t>&, ExceptionState&);
  void identifyInputsAndOutputs(Vector<uint32_t>&, Vector<uint32_t>&, ExceptionState&);
  ScriptPromise finish(ScriptState*);
  ScriptPromise createCompilation(ScriptState*);

  void Trace(blink::Visitor*);

 private:
  void OnResultCode(ScriptPromiseResolver*, const String&, int32_t);
  void OnCreateCompilation(ScriptPromiseResolver*, int32_t, ml::mojom::blink::CompilationInitParamsPtr);
  void OnConnectionError();

 private:
  bool is_finished_;
  ml::mojom::blink::ModelPtr model_;
  ml::mojom::blink::ModelInfoPtr model_info_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  HeapVector<Member<DOMArrayBufferView>> buffer_views_;
};

}  // namespace blink

#endif  // Model_h