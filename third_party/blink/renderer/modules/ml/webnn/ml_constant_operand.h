// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_CONSTANT_OPERAND_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_CONSTANT_OPERAND_H_

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLGraphBuilder;

// Represents an `MLOperand` created from the `MLGraphBuilder.constant()`
// method. See https://www.w3.org/TR/webnn/#api-mlgraphbuilder-constant.
class MODULES_EXPORT MLConstantOperand final : public MLOperand {
 public:
  // Creates a constant operand which is backed by a copy of `bytes`. The length
  // of `bytes` must match the number of bytes described by `descriptor`.
  MLConstantOperand(MLGraphBuilder* builder,
                    webnn::OperandDescriptor descriptor,
                    base::span<const uint8_t> bytes);

  MLConstantOperand(const MLConstantOperand&) = delete;
  MLConstantOperand& operator=(const MLConstantOperand&) = delete;

  ~MLConstantOperand() override;

  void Trace(Visitor* visitor) const override;

  base::span<const uint8_t> Bytes() const;

  void ReleaseBytes();

 private:
  // Bytes associated with a constant operand. See
  // https://www.w3.org/TR/webnn/#dom-mlgraphbuilder-constant.
  //
  // Once these bytes have been copied to the respective graph, they should be
  // released via `ReleaseBytes()` to avoid keeping this copy unnecessarily.
  base::HeapArray<uint8_t> constant_bytes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_CONSTANT_OPERAND_H_
