// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TYPE_CONVERTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TYPE_CONVERTER_H_

#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class MLOperand;
class MLOperator;
base::expected<webnn::mojom::blink::OperatorPtr, String> ConvertToMojoOperator(
    const HeapHashMap<Member<const MLOperand>, uint64_t>& operand_to_id_map,
    const MLOperator* op);

}  // namespace blink

namespace mojo {

template <>
struct TypeConverter<webnn::mojom::blink::OperandPtr, blink::MLOperand*> {
  static webnn::mojom::blink::OperandPtr Convert(
      const blink::MLOperand* ml_operand);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TYPE_CONVERTER_H_
