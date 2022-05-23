// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLContext;
class MLClampOptions;
class MLConv2dOptions;
class MLGemmOptions;
class MLPool2dOptions;
class MLOperand;
class MLOperandDescriptor;
class MLOperator;

typedef HeapVector<std::pair<String, Member<MLOperand>>> MLNamedOperands;

class MLGraphBuilder final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MLGraphBuilder* Create(MLContext* context);

  explicit MLGraphBuilder(MLContext* context);

  MLGraphBuilder(const MLGraphBuilder&) = delete;
  MLGraphBuilder& operator=(const MLGraphBuilder&) = delete;

  ~MLGraphBuilder() override;

  void Trace(Visitor* visitor) const override;

  // ml_graph_builder.idl
  MLOperand* input(String name, const MLOperandDescriptor* desc);
  MLOperand* constant(const MLOperandDescriptor* desc,
                      NotShared<DOMArrayBufferView> buffer_view);

  // The order of operations declaration is the same as spec.
  MLOperand* clamp(const MLOperand*, const MLClampOptions*);
  MLOperator* clamp(const MLClampOptions*);

  MLOperand* conv2d(const MLOperand*, const MLOperand*, const MLConv2dOptions*);

  // Element-wise binary operations
  MLOperand* add(const MLOperand*, const MLOperand*);

  MLOperand* gemm(const MLOperand*, const MLOperand*, const MLGemmOptions*);

  // Pooling operations
  MLOperand* averagePool2d(const MLOperand*, const MLPool2dOptions*);

  MLOperand* reshape(const MLOperand*, const Vector<int32_t>&);

  MLOperand* softmax(const MLOperand*);

 private:
  Member<MLContext> ml_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_BUILDER_H_
