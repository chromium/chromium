// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Model_h
#define Model_h

#include "platform/bindings/ScriptWrappable.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "core/typed_arrays/DOMTypedArray.h"

namespace blink {

class OperandOptions;
class ExceptionState;
class Compilation;

class Model final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Model();
  ~Model() override;

  uint32_t addOperand(const OperandOptions& options, ExceptionState& state);
  void setOperandValue(uint32_t index, MaybeShared<DOMArrayBufferView> data, ExceptionState& state);
  void addOperation(uint32_t type, Vector<uint32_t>& inputs, Vector<uint32_t>& outputs, ExceptionState& state);
  void identifyInputsAndOutputs(Vector<uint32_t>& inputs, Vector<uint32_t>& outputs, ExceptionState& state);
  void finish(ExceptionState& state);
  Compilation* createCompilation();

  void Trace(blink::Visitor*);
};

}  // namespace blink

#endif  // Model_h