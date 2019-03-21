// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_ML_H_

#include "services/ml/public/mojom/model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/operand_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Model final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit Model(ml::mojom::blink::ModelPtrInfo);
  ~Model() override;

  void addOperand(const OperandOptions*, ExceptionState&);
  void setOperandValue(uint32_t,
                       MaybeShared<DOMArrayBufferView>,
                       ExceptionState&);
  void addOperation(int32_t,
                    Vector<uint32_t>&,
                    Vector<uint32_t>&,
                    ExceptionState&);
  void identifyInputsAndOutputs(Vector<uint32_t>&,
                                Vector<uint32_t>&,
                                ExceptionState&);
  ScriptPromise finish(ScriptState*);
  ScriptPromise createCompilation(ScriptState*);

  void Trace(blink::Visitor*) override;

 private:
  void OnResultCode(ScriptPromiseResolver*, const String&, int32_t);
  void OnCreateCompilation(ScriptPromiseResolver*,
                           int32_t,
                           ml::mojom::blink::CompilationInitParamsPtr);
  void OnConnectionError();

  bool is_finished_;
  ml::mojom::blink::ModelPtr model_;
  ml::mojom::blink::ModelInfoPtr model_info_;
  mojo::ScopedSharedBufferHandle memory_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  HeapHashMap<WTF::String, Member<DOMArrayBufferView>> buffer_views_;
};

}  // namespace blink

#endif  // Model_h
