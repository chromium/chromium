// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Model_h
#define Model_h

#include "platform/bindings/ScriptWrappable.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "platform/wtf/Vector.h"
#include "modules/ml/OperandOptions.h"
#include "services/ml/public/interfaces/neuralnetwork.mojom-blink.h"

namespace blink {

class ExceptionState;
class Compilation;

class Model final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Model();
  ~Model() override;

  bool IsFinished() { return is_finished_; }

  uint32_t addOperand(const OperandOptions&, ExceptionState&);
  void setOperandValue(uint32_t, MaybeShared<DOMArrayBufferView>, ExceptionState&);
  void addOperation(int32_t, Vector<uint32_t>&, Vector<uint32_t>&, ExceptionState&);
  void identifyInputsAndOutputs(Vector<uint32_t>&, Vector<uint32_t>&, ExceptionState&);
  void finish(ExceptionState&);

  void Trace(blink::Visitor*);

 private:
  friend Compilation;
  bool is_finished_;

  ml::mojom::blink::ModelPtr mojo_model_;

  Vector<uint32_t> buffer_view_indexes_;
  HeapVector<Member<DOMArrayBufferView>> buffer_views_;
};

}  // namespace blink

#endif  // Model_h