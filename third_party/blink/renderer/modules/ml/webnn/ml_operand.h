// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERAND_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_OPERAND_H_

#include <optional>

#include "base/types/expected.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLGraphBuilder;
class MLOperator;

DOMArrayBufferView::ViewType GetArrayBufferViewType(
    V8MLOperandDataType::Enum operand_type);

class MODULES_EXPORT MLOperand final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Represents an MLOperandDescriptor whose characteristics have already been
  // validated according to the steps specified in
  // https://www.w3.org/TR/webnn/#mloperanddescriptor-check-dimensions.
  class ValidatedDescriptor {
   public:
    // Creates a ValidatedDescriptor or returns an error message which may be
    // used to throw a TypeError if the inputs are not valid.
    [[nodiscard]] static base::expected<ValidatedDescriptor, String> Create(
        V8MLOperandDataType::Enum data_type,
        Vector<uint32_t> dimensions);

    V8MLOperandDataType::Enum DataType() const { return data_type_; }
    const Vector<uint32_t>& Dimensions() const { return dimensions_; }
    size_t ByteLength() const;
    size_t NumberOfElements() const;

   private:
    ValidatedDescriptor(V8MLOperandDataType::Enum data_type,
                        Vector<uint32_t> dimensions);

    const V8MLOperandDataType::Enum data_type_;
    const Vector<uint32_t> dimensions_;
  };

  // Validate and create different kinds of operand if there are no errors.
  // Otherwise return an error message which may be used
  // to throw a TypeError if the inputs are not valid.
  static base::expected<MLOperand*, String> ValidateAndCreateInput(
      MLGraphBuilder* builder,
      V8MLOperandDataType::Enum data_type,
      Vector<uint32_t> dimensions,
      String name);
  static base::expected<MLOperand*, String> ValidateAndCreateConstant(
      MLGraphBuilder* builder,
      V8MLOperandDataType::Enum data_type,
      Vector<uint32_t> dimensions,
      const DOMArrayBufferView* array_buffer_view);
  static base::expected<MLOperand*, String> ValidateAndCreateOutput(
      MLGraphBuilder* builder,
      V8MLOperandDataType::Enum data_type,
      Vector<uint32_t> dimensions,
      const MLOperator* ml_operator);

  // The constructor shouldn't be called directly. The callers should use
  // Create* methods instead.
  MLOperand(MLGraphBuilder* builder,
            webnn::mojom::blink::Operand::Kind kind,
            ValidatedDescriptor descriptor);

  MLOperand(const MLOperand&) = delete;
  MLOperand& operator=(const MLOperand&) = delete;

  ~MLOperand() override;

  void Trace(Visitor* visitor) const override;

  MLGraphBuilder* Builder() const;
  webnn::mojom::blink::Operand::Kind Kind() const;
  V8MLOperandDataType::Enum DataType() const;
  const Vector<uint32_t>& Dimensions() const;
  const String& Name() const;
  const DOMArrayBufferView* ArrayBufferView() const;
  const MLOperator* Operator() const;

  // The total number of elements in the operand. Its value is the product of
  // all values of the dimensions. For scalar operand, the number of elements
  // is 1.
  size_t NumberOfElements() const;

  // The byte length of the oprand. It is defined by WebNN spec as:
  // https://www.w3.org/TR/webnn/#mloperanddescriptor-byte-length
  size_t ByteLength() const;

  // IDL interface:
  V8MLOperandDataType dataType() const;
  Vector<uint32_t> shape() const;

 private:
  Member<MLGraphBuilder> builder_;

  const webnn::mojom::blink::Operand::Kind kind_;

  // Represents a valid MLOperandDescriptor.
  // https://www.w3.org/TR/webnn/#dictdef-mloperanddescriptor
  const ValidatedDescriptor descriptor_;

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
