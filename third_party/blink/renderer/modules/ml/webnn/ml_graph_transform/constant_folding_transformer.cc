// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/constant_folding_transformer.h"

#include "services/webnn/public/cpp/operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_constant_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

namespace blink_mojom = webnn::mojom::blink;

namespace blink {

void ConstantFoldingTransformer::Transform(MLNamedOperands& named_outputs) {
  HeapVector<Member<MLOperator>> sorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);
  HeapHashSet<Member<const MLOperator>> graph_output_operators =
      GetGraphOutputOperators(named_outputs);

  for (auto& op : sorted_operators) {
    if (!graph_output_operators.Contains(op)) {
      TryFoldConstant(*op);
    }
  }
}

void ConstantFoldingTransformer::TryFoldConstant(MLOperator& op) {
  if (op.Kind() != blink_mojom::Operation::Tag::kTranspose &&
      op.Kind() != blink_mojom::Operation::Tag::kReshape) {
    return;
  }
  MLOperand* input = op.Inputs()[0];
  if (input->Kind() != blink_mojom::Operand::Kind::kConstant) {
    return;
  }
  MLOperand* output = op.Outputs()[0];
  if (input->DependentOperators().size() != 1 ||
      output->DependentOperators().size() != 1) {
    return;
  }
  MLConstantOperand* constant_operand = input->AsConstantOperand();
  if (op.Kind() == blink_mojom::Operation::Tag::kTranspose) {
    // TODO(crbug.com/428232161): Support sub byte transposes.
    if (webnn::OperandDescriptor::GetBitsPerElement(input->DataType()) < 8u) {
      return;
    }
    // TODO(crbug.com/428232161): Support transposing constant tensors.
    if (constant_operand->tensor()) {
      return;
    }
  }
  MLConstantOperand* new_constant_operand =
      ReplaceConstantOperandWithNewShape(constant_operand, output->shape());
  RemoveUnaryOperator(&op);
  if (op.Kind() == blink_mojom::Operation::Tag::kReshape) {
    return;
  }

  // If the op is transpose, apply permutation to the new constant.
  auto* options = static_cast<const MLTransposeOptions*>(op.Options());
  wtf_size_t rank = input->Rank();
  Vector<uint32_t> default_permutation = CreateDefaultPermutation(rank);
  Vector<uint32_t> permutation =
      options->getPermutationOr(std::move(default_permutation));
  ApplyPermutation(constant_operand, new_constant_operand,
                   std::move(permutation));
}

void ConstantFoldingTransformer::ApplyPermutation(
    MLConstantOperand* old_constant,
    MLConstantOperand* new_constant,
    Vector<uint32_t> permutation) {
  base::span<const uint32_t> previous_permutation =
      old_constant->Descriptor().pending_permutation();
  if (previous_permutation.empty()) {
    new_constant->SetPendingPermutation(permutation);
    return;
  }
  Vector<uint32_t> squashed_permutation(permutation.size());
  for (size_t i = 0; i < permutation.size(); ++i) {
    // The new permutation maps index 'i' to
    // previous_permutation[permutation[i]]
    squashed_permutation[i] = previous_permutation[permutation[i]];
  }
  new_constant->SetPendingPermutation(squashed_permutation);
}

}  // namespace blink
