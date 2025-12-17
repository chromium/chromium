// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_UTILS_ML_GRAPH_DUMP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_UTILS_ML_GRAPH_DUMP_H_

#include "base/values.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT MLGraphDumper : public GarbageCollected<MLGraphDumper> {
  class NodeIdMapper : public GarbageCollected<NodeIdMapper> {
   public:
    void Trace(Visitor* visitor) const;
    wtf_size_t NextId(Member<const MLOperator> op);
    wtf_size_t NextId(Member<const MLOperand> operand);
    wtf_size_t NextId(const String& graph_output_name);

   private:
    HeapHashMap<Member<const MLOperator>, int>
        op_to_id_map_;  // For operator node
    HeapHashMap<Member<const MLOperand>, int>
        input_constant_operand_to_id_map_;  // For input, constant node

    HashMap<String, int> graph_output_name_to_id_map_;  // For graph output

    wtf_size_t NextNewId();
  };

 public:
  MLGraphDumper();

  void Trace(Visitor* visitor) const;

  void RecordGraph(const std::string& graph_id,
                   const MLNamedOperands& named_outputs);

  const base::Value::Dict& GetRoot() const { return root_; }

 private:
  base::Value::Dict root_;
  Member<NodeIdMapper> node_id_mapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TRANSFORM_UTILS_ML_GRAPH_DUMP_H_
