// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERAND_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERAND_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLGraphBuilder;
class MLOperator;

class MLOperand final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum OperandKind { kInput, kConstant, kOutput };

  static MLOperand* CreateInput(MLGraphBuilder* builder,
                                const V8MLOperandType::Enum type,
                                Vector<int32_t> dimensions,
                                String name);
  static MLOperand* CreateConstant(MLGraphBuilder* builder,
                                   const V8MLOperandType::Enum type,
                                   Vector<int32_t> dimensions,
                                   const DOMArrayBufferView* array_buffer_view);
  static MLOperand* CreateOutput(MLGraphBuilder* builder,
                                 const V8MLOperandType::Enum type,
                                 Vector<int32_t> dimensions,
                                 const MLOperator* ml_operator);

  // The constructor shouldn't be called directly. The callers should use
  // Create* methods instead.
  MLOperand(MLGraphBuilder* builder,
            OperandKind kind,
            const V8MLOperandType::Enum type,
            Vector<int32_t> dimensions);

  MLOperand(const MLOperand&) = delete;
  MLOperand& operator=(const MLOperand&) = delete;

  ~MLOperand() override;

  void Trace(Visitor* visitor) const override;

  MLGraphBuilder* Builder() const;
  OperandKind Kind() const;
  V8MLOperandType::Enum Type() const;
  const Vector<int32_t>& Dimensions() const;
  const String& Name() const;
  const DOMArrayBufferView* ArrayBufferView() const;
  const MLOperator* Operator() const;

 private:
  Member<MLGraphBuilder> builder_;
  OperandKind kind_;
  V8MLOperandType::Enum type_;
  // The dimensions of the operand. For scalar value, set {1}.
  Vector<int32_t> dimensions_;
  // The name of input operand. According to
  // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-input, only input operand
  // is created with a name.
  String name_;
  // The buffer view of constant operand. According to
  // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-constant, only constant
  // operand is associated with an array buffer view that contains the
  // user-supplied constant data.
  Member<const DOMArrayBufferView> array_buffer_view_;
  // The operator that produces the output operand. Only output operand has an
  // operator that produces the operand by an operator build method of
  // MLGraphBuilder interface.
  Member<const MLOperator> operator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERAND_H_
