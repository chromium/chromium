// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TYPE_CONVERTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TYPE_CONVERTER_H_

#include <optional>

#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/webnn/public/cpp/ml_number.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class MLOperand;
class MLOperator;
class V8UnionBigintOrUnrestrictedDouble;

// Add operand to `graph_info` and return its operand id.
webnn::OperandId AddOperand(webnn::mojom::blink::GraphInfo& graph_info,
                            webnn::mojom::blink::OperandPtr operand);

void SerializeMojoOperation(
    const HeapHashMap<Member<const MLOperand>, webnn::OperandId>&
        operand_to_id_map,
    const webnn::ContextProperties& context_properties,
    const MLOperator* op,
    webnn::mojom::blink::GraphInfo* graph_info);

base::expected<webnn::MLNumber, String> ToMLNumberAsType(
    const V8UnionBigintOrUnrestrictedDouble& number,
    webnn::OperandDataType type);

}  // namespace blink

namespace mojo {

template <>
struct TypeConverter<webnn::mojom::blink::OperandPtr, blink::MLOperand*> {
  static webnn::mojom::blink::OperandPtr Convert(
      const blink::MLOperand* ml_operand);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TYPE_CONVERTER_H_
