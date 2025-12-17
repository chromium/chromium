// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/utils/ml_graph_dump.h"

#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "fp16/fp16.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_arg_min_max_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_cumulative_sum_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_hard_sigmoid_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_instance_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_layer_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_linear_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operator_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_recurrent_network_activation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reverse_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_scatter_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_slice_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_triangular_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

wtf_size_t GetOutputIndex(MLOperator* op, MLOperand* operand) {
  for (wtf_size_t i = 0; i < op->Outputs().size(); ++i) {
    if (op->Outputs()[i] == operand) {
      return i;
    }
  }
  NOTREACHED() << "Operand is not an output of the operator.";
}

Vector<Vector<uint32_t>> GetShapesOfOperatorOutputPorts(const MLOperator* op) {
  Vector<Vector<uint32_t>> shapes;
  for (const auto& output : op->Outputs()) {
    shapes.push_back(output->Shape());
  }
  return shapes;
}

Vector<V8MLOperandDataType> GetDataTypesOfOperatorOutputPorts(
    const MLOperator* op) {
  Vector<V8MLOperandDataType> data_types;
  for (const auto& output : op->Outputs()) {
    data_types.push_back(output->dataType());
  }
  return data_types;
}

std::string GetTensorShapeString(const Vector<uint32_t>& shape) {
  std::string result = "tensor<";
  for (wtf_size_t i = 0; i < shape.size(); ++i) {
    result += base::NumberToString(shape[i]);
    if (i != shape.size() - 1) {
      result += "x";
    }
  }
  result += ">";
  return result;
}

template <typename T>
concept HasAsStringMember = requires(T t) {
  { t.AsString().Utf8() } -> std::convertible_to<std::string>;
};

template <typename T>
concept HasUtf8Member = requires(T t) {
  { t.Utf8() } -> std::convertible_to<std::string>;
};

template <typename T>
concept IsNumberStringable = requires(T t) {
  { base::NumberToString(t) } -> std::convertible_to<std::string>;
};

template <typename T>
std::string ToAttributeString(T&& t) {
  using DecayT = std::decay_t<T>;
  if constexpr (std::is_convertible_v<T, std::string>) {
    return std::string(t);
  } else if constexpr (std::is_same_v<DecayT, bool>) {
    return t ? "true" : "false";
  } else if constexpr (HasUtf8Member<T>) {
    return t.Utf8();
  } else if constexpr (HasAsStringMember<T>) {
    return t.AsString().Utf8();
  } else if constexpr (IsNumberStringable<T>) {
    return base::NumberToString(t);
  } else if constexpr (requires {
                         t.size();
                         t[0];
                       }) {  // Vector-like
    std::string result;
    for (wtf_size_t i = 0; i < t.size(); ++i) {
      result += ToAttributeString(t[i]);
      if (i != t.size() - 1) {
        result += ", ";
      }
    }
    return result;
  } else {
    static_assert(false, "Type T is not stringable");
  }
}

std::string MLNumberToString(const webnn::MLNumber& number,
                             webnn::OperandDataType dtype) {
  switch (dtype) {
    case webnn::OperandDataType::kFloat32: {
      return base::NumberToString(number.AsFloat32());
    }
    case webnn::OperandDataType::kFloat16: {
      return base::NumberToString(fp16_ieee_to_fp32_value(number.AsFloat16()));
    }
    case webnn::OperandDataType::kInt32: {
      return base::NumberToString(number.AsInt32());
    }
    case webnn::OperandDataType::kInt64: {
      return base::NumberToString(number.AsInt64());
    }
    case webnn::OperandDataType::kUint32: {
      return base::NumberToString(number.AsUint32());
    }
    case webnn::OperandDataType::kUint64: {
      return base::NumberToString(number.AsUint64());
    }
    case webnn::OperandDataType::kInt8: {
      return base::NumberToString(number.AsInt8());
    }
    case webnn::OperandDataType::kUint8: {
      return base::NumberToString(number.AsUint8());
    }
    case webnn::OperandDataType::kInt4: {
      return base::NumberToString(number.AsInt8());
    }
    case webnn::OperandDataType::kUint4: {
      return base::NumberToString(number.AsUint8());
    }
  }
}

struct NodeAttribute {
  std::string key;
  std::string value;

  template <typename T>
  NodeAttribute(const std::string& k, T&& v)
      : key(k), value(ToAttributeString(std::forward<T>(v))) {}
};

struct IncomingEdge {
  IncomingEdge(wtf_size_t source_id,
               wtf_size_t source_output_id,
               wtf_size_t target_input_id)
      : source_node_id(base::NumberToString(source_id)),
        source_node_output_id(base::NumberToString(source_output_id)),
        target_node_input_id(base::NumberToString(target_input_id)) {}

  std::string source_node_id;
  std::string source_node_output_id;
  std::string target_node_input_id;
};

// This Node class is designed to match `GraphNode` in Model Explorer
// https://github.com/google-ai-edge/model-explorer/blob/model-explorer-v0.1.29/src/ui/src/components/visualizer/common/input_graph.ts#L148
class Node : public GarbageCollected<Node> {
 public:
  Node(wtf_size_t id,
       const std::string_view& opkind,
       const std::string_view& label,
       const Vector<Vector<uint32_t>>& output_shapes,
       const Vector<V8MLOperandDataType>& output_data_types)
      : id_(id),
        opkind_(opkind),
        label_(label),
        output_shapes_(output_shapes),
        output_data_types_(output_data_types) {}

  void Trace(Visitor* visitor) const {}

  void AppendInputEdge(const IncomingEdge& edge) {
    incoming_edges_.push_back(edge);
  }

  template <typename T>
  void SetAttribute(const std::string key, const T value) {
    attributes_.push_back(NodeAttribute{key, value});
  }

  void SetOpAttributes(const MLOperator* op) {
    switch (op->Kind()) {
      case webnn::mojom::blink::Operation::Tag::kArgMinMax: {
        SetArgMinMaxAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kBatchNormalization: {
        SetBatchNormalizationAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kClamp: {
        SetClampAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kConcat: {
        SetConcatAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kConv2d: {
        SetConv2dAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kCumulativeSum: {
        SetCumulativeSumAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kElu: {
        SetEluAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kGather: {
        SetGatherAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kGatherElements: {
        SetGatherElementAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kGemm: {
        SetGemmAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kGru: {
        SetGruAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kGruCell: {
        SetGruCellAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kHardSigmoid: {
        SetHardSigmoidAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kInstanceNormalization: {
        SetInstanceNormalizationAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kLayerNormalization: {
        SetLayerNormalizationAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kLeakyRelu: {
        SetLeakyReluAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kLinear: {
        SetLinearAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kLstm: {
        SetLstmAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kLstmCell: {
        SetLstmCellAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kPad: {
        SetPadAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kPool2d: {
        SetPool2dAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kReduce: {
        SetReduceAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kResample2d: {
        SetResample2dAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kReverse: {
        SetReverseAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kScatterElements: {
        SetScatterElementsAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kScatterNd: {
        SetScatterNdAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kSlice: {
        SetSliceAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kSoftmax: {
        SetSoftmaxAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kSplit: {
        SetSplitAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kTile: {
        SetTileAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kTranspose: {
        SetTransposeAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kTriangular: {
        SetTriangularAttributes(op);
        break;
      }
      case webnn::mojom::blink::Operation::Tag::kDequantizeLinear:
      case webnn::mojom::blink::Operation::Tag::kElementWiseBinary:
      case webnn::mojom::blink::Operation::Tag::kElementWiseUnary:
      case webnn::mojom::blink::Operation::Tag::kExpand:
      case webnn::mojom::blink::Operation::Tag::kGatherNd:
      case webnn::mojom::blink::Operation::Tag::kGelu:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kHardSwish:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kMatmul:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kPrelu:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::
          kQuantizeLinear:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kRelu:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kReshape:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kSigmoid:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kSoftplus:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kSoftsign:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kTanh:
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kWhere: {
        // No attributes to set.
        break;
      }
    }
  }

  wtf_size_t Id() const { return id_; }
  const Vector<IncomingEdge>& IncomingEdges() const { return incoming_edges_; }
  const Vector<NodeAttribute>& Attributes() const { return attributes_; }

  base::Value::Dict ToJson() const {
    base::Value::Dict node_json;
    node_json.Set("id", base::NumberToString(Id()));
    node_json.Set("label", opkind_);
    node_json.Set("namespace", label_);

    base::Value::List node_attrs_json;
    for (const auto& attr : attributes_) {
      base::Value::Dict node_attr_json;
      node_attr_json.Set("key", attr.key);
      node_attr_json.Set("value", attr.value);
      node_attrs_json.Append(std::move(node_attr_json));
    }
    node_json.Set("attrs", std::move(node_attrs_json));

    base::Value::List incoming_edges_json;
    for (const auto& edge : incoming_edges_) {
      base::Value::Dict edge_json;
      edge_json.Set("sourceNodeId", edge.source_node_id);
      edge_json.Set("sourceNodeOutputId", edge.source_node_output_id);
      edge_json.Set("targetNodeInputId", edge.target_node_input_id);
      incoming_edges_json.Append(std::move(edge_json));
    }
    node_json.Set("incomingEdges", std::move(incoming_edges_json));

    base::Value::List outputs_metadata_json;
    for (wtf_size_t i = 0; i < output_shapes_.size(); ++i) {
      base::Value::Dict output_metadata_json;
      output_metadata_json.Set("id", base::NumberToString(i));

      base::Value::List attrs_json;

      base::Value::Dict shape_attr;
      shape_attr.Set("key", "tensor_shape");
      shape_attr.Set("value", GetTensorShapeString(output_shapes_[i]));
      attrs_json.Append(std::move(shape_attr));

      base::Value::Dict data_type_attr;
      data_type_attr.Set("key", "dtype");
      data_type_attr.Set("value", output_data_types_[i].AsString().Utf8());
      attrs_json.Append(std::move(data_type_attr));

      output_metadata_json.Set("attrs", std::move(attrs_json));
      outputs_metadata_json.Append(std::move(output_metadata_json));
    }
    node_json.Set("outputsMetadata", std::move(outputs_metadata_json));

    return node_json;
  }

 private:
  void SetArgMinMaxAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kArgMinMax);
    const MLArgMinMaxOperator* argminmax_op =
        static_cast<const MLArgMinMaxOperator*>(op);

    uint32_t axis = argminmax_op->Axis();
    attributes_.push_back(NodeAttribute{"axis", axis});

    const MLArgMinMaxOptions* options =
        static_cast<const MLArgMinMaxOptions*>(op->Options());

    if (options->hasKeepDimensions()) {
      attributes_.push_back(
          NodeAttribute{"keepDimensions", options->keepDimensions()});
    }
    if (options->hasOutputDataType()) {
      attributes_.push_back(
          NodeAttribute{"outputDataType", options->outputDataType()});
    }
  }

  void SetBatchNormalizationAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(),
             webnn::mojom::blink::Operation::Tag::kBatchNormalization);
    const MLBatchNormalizationOptions* options =
        static_cast<const MLBatchNormalizationOptions*>(op->Options());

    if (options->hasAxis()) {
      attributes_.push_back(NodeAttribute{"axis", options->axis()});
    }
    if (options->hasEpsilon()) {
      attributes_.push_back(NodeAttribute{"epsilon", options->epsilon()});
    }
  }

  void SetClampAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kClamp);
    const MLClampOperator* clamp_op = static_cast<const MLClampOperator*>(op);

    attributes_.push_back(NodeAttribute{
        "maxValue", MLNumberToString(clamp_op->max_value(),
                                     clamp_op->Inputs()[0]->DataType())});
    attributes_.push_back(NodeAttribute{
        "minValue", MLNumberToString(clamp_op->min_value(),
                                     clamp_op->Inputs()[0]->DataType())});
  }

  void SetConcatAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kConcat);
    const MLConcatOperator* concat_op =
        static_cast<const MLConcatOperator*>(op);

    uint32_t axis = concat_op->Axis();
    attributes_.push_back(NodeAttribute{"axis", axis});
  }

  void SetConv2dAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kConv2d);
    auto conv2d_kind =
        std::get<webnn::mojom::blink::Conv2d::Kind>(op->SubKind());

    switch (conv2d_kind) {
      case webnn::mojom::blink::Conv2d::Kind::kDirect: {
        const MLConv2dOptions* options =
            static_cast<const MLConv2dOptions*>(op->Options());

        if (options->hasPadding()) {
          attributes_.push_back(NodeAttribute{"padding", options->padding()});
        }
        if (options->hasStrides()) {
          attributes_.push_back(NodeAttribute{"strides", options->strides()});
        }
        if (options->hasDilations()) {
          attributes_.push_back(
              NodeAttribute{"dilations", options->dilations()});
        }
        if (options->hasGroups()) {
          attributes_.push_back(NodeAttribute{"groups", options->groups()});
        }
        if (options->hasInputLayout()) {
          attributes_.push_back(
              NodeAttribute{"inputLayout", options->inputLayout()});
        }
        if (options->hasFilterLayout()) {
          attributes_.push_back(
              NodeAttribute{"filterLayout", options->filterLayout()});
        }
        break;
      }
      case webnn::mojom::blink::Conv2d::Kind::kTransposed: {
        const MLConvTranspose2dOptions* options =
            static_cast<const MLConvTranspose2dOptions*>(op->Options());
        if (options->hasPadding()) {
          attributes_.push_back(NodeAttribute{"padding", options->padding()});
        }
        if (options->hasStrides()) {
          attributes_.push_back(NodeAttribute{"strides", options->strides()});
        }
        if (options->hasDilations()) {
          attributes_.push_back(
              NodeAttribute{"dilations", options->dilations()});
        }
        if (options->hasOutputPadding()) {
          attributes_.push_back(
              NodeAttribute{"outputPadding", options->outputPadding()});
        }
        if (options->hasOutputSizes()) {
          attributes_.push_back(
              NodeAttribute{"outputSizes", options->outputSizes()});
        }
        if (options->hasGroups()) {
          attributes_.push_back(NodeAttribute{"groups", options->groups()});
        }
        if (options->hasInputLayout()) {
          attributes_.push_back(
              NodeAttribute{"inputLayout", options->inputLayout()});
        }
        if (options->hasFilterLayout()) {
          attributes_.push_back(
              NodeAttribute{"filterLayout", options->filterLayout()});
        }
        break;
      }
    }
  }

  void SetCumulativeSumAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kCumulativeSum);

    MLCumulativeSumOperator* cumsum_op =
        static_cast<MLCumulativeSumOperator*>(const_cast<MLOperator*>(op));
    const MLCumulativeSumOptions* options =
        static_cast<const MLCumulativeSumOptions*>(cumsum_op->Options());

    attributes_.push_back(NodeAttribute{"axis", cumsum_op->Axis()});
    if (options->hasExclusive()) {
      attributes_.push_back(NodeAttribute{"exclusive", options->exclusive()});
    }
    if (options->hasReversed()) {
      attributes_.push_back(NodeAttribute{"reversed", options->reversed()});
    }
  }

  void SetEluAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kElu);
    const MLEluOptions* options =
        static_cast<const MLEluOptions*>(op->Options());

    if (options->hasAlpha()) {
      attributes_.push_back(NodeAttribute{"alpha", options->alpha()});
    }
  }

  void SetGatherAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kGather);
    const MLGatherOptions* options =
        static_cast<const MLGatherOptions*>(op->Options());

    if (options->hasAxis()) {
      attributes_.push_back(NodeAttribute{"axis", options->axis()});
    }
  }

  void SetGatherElementAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kGatherElements);
    const MLGatherOptions* options =
        static_cast<const MLGatherOptions*>(op->Options());

    if (options->hasAxis()) {
      attributes_.push_back(NodeAttribute{"axis", options->axis()});
    }
  }

  void SetGemmAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kGemm);
    const MLGemmOptions* options =
        static_cast<const MLGemmOptions*>(op->Options());

    if (options->hasAlpha()) {
      attributes_.push_back(NodeAttribute{"alpha", options->alpha()});
    }
    if (options->hasBeta()) {
      attributes_.push_back(NodeAttribute{"beta", options->beta()});
    }
    if (options->hasATranspose()) {
      attributes_.push_back(NodeAttribute{"aTranspose", options->aTranspose()});
    }
    if (options->hasBTranspose()) {
      attributes_.push_back(NodeAttribute{"bTranspose", options->bTranspose()});
    }
  }

  void SetGruAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kGru);
    const MLGruOptions* options =
        static_cast<const MLGruOptions*>(op->Options());

    if (options->hasDirection()) {
      attributes_.push_back(NodeAttribute{"direction", options->direction()});
    }
    if (options->hasLayout()) {
      attributes_.push_back(NodeAttribute{"layout", options->layout()});
    }
    if (options->hasResetAfter()) {
      attributes_.push_back(NodeAttribute{"resetAfter", options->resetAfter()});
    }
    if (options->hasReturnSequence()) {
      attributes_.push_back(
          NodeAttribute{"returnSequence", options->returnSequence()});
    }

    if (options->hasActivations()) {
      const Vector<V8MLRecurrentNetworkActivation>& activations =
          options->activations();
      if (!activations.empty()) {
        attributes_.push_back(NodeAttribute{"activations", activations});
      }
    }
  }

  void SetGruCellAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kGruCell);
    const MLGruCellOptions* options =
        static_cast<const MLGruCellOptions*>(op->Options());

    if (options->hasLayout()) {
      attributes_.push_back(NodeAttribute{"layout", options->layout()});
    }
    if (options->hasResetAfter()) {
      attributes_.push_back(NodeAttribute{"resetAfter", options->resetAfter()});
    }

    if (options->hasActivations()) {
      const Vector<V8MLRecurrentNetworkActivation>& activations =
          options->activations();
      if (!activations.empty()) {
        attributes_.push_back(NodeAttribute{"activations", activations});
      }
    }
  }

  void SetHardSigmoidAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kHardSigmoid);
    const MLHardSigmoidOptions* options =
        static_cast<const MLHardSigmoidOptions*>(op->Options());

    if (options->hasAlpha()) {
      attributes_.push_back(NodeAttribute{"alpha", options->alpha()});
    }
    if (options->hasBeta()) {
      attributes_.push_back(NodeAttribute{"beta", options->beta()});
    }
  }

  void SetLayerNormalizationAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(),
             webnn::mojom::blink::Operation::Tag::kLayerNormalization);
    const MLLayerNormalizationOptions* options =
        static_cast<const MLLayerNormalizationOptions*>(op->Options());

    if (options->hasAxes()) {
      attributes_.push_back(NodeAttribute{"axes", options->axes()});
    }
    if (options->hasEpsilon()) {
      attributes_.push_back(NodeAttribute{"epsilon", options->epsilon()});
    }
  }

  void SetInstanceNormalizationAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(),
             webnn::mojom::blink::Operation::Tag::kInstanceNormalization);
    const MLInstanceNormalizationOptions* options =
        static_cast<const MLInstanceNormalizationOptions*>(op->Options());

    if (options->hasEpsilon()) {
      attributes_.push_back(NodeAttribute{"epsilon", options->epsilon()});
    }
    if (options->hasLayout()) {
      attributes_.push_back(NodeAttribute{"layout", options->layout()});
    }
  }

  void SetLeakyReluAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kLeakyRelu);
    const MLLeakyReluOptions* options =
        static_cast<const MLLeakyReluOptions*>(op->Options());

    if (options->hasAlpha()) {
      attributes_.push_back(NodeAttribute{"alpha", options->alpha()});
    }
  }

  void SetLinearAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kLinear);
    const MLLinearOptions* options =
        static_cast<const MLLinearOptions*>(op->Options());

    if (options->hasAlpha()) {
      attributes_.push_back(NodeAttribute{"alpha", options->alpha()});
    }
    if (options->hasBeta()) {
      attributes_.push_back(NodeAttribute{"beta", options->beta()});
    }
  }

  void SetLstmAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kLstm);
    const MLLstmOptions* options =
        static_cast<const MLLstmOptions*>(op->Options());

    if (options->hasDirection()) {
      attributes_.push_back(NodeAttribute{"direction", options->direction()});
    }
    if (options->hasLayout()) {
      attributes_.push_back(NodeAttribute{"layout", options->layout()});
    }
    if (options->hasReturnSequence()) {
      attributes_.push_back(
          NodeAttribute{"returnSequence", options->returnSequence()});
    }
    if (options->hasActivations()) {
      const Vector<V8MLRecurrentNetworkActivation>& activations =
          options->activations();
      if (!activations.empty()) {
        attributes_.push_back(NodeAttribute{"activations", activations});
      }
    }
  }

  void SetLstmCellAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kLstmCell);
    const MLLstmCellOptions* options =
        static_cast<const MLLstmCellOptions*>(op->Options());

    if (options->hasLayout()) {
      attributes_.push_back(NodeAttribute{"layout", options->layout()});
    }
    if (options->hasActivations()) {
      const Vector<V8MLRecurrentNetworkActivation>& activations =
          options->activations();
      if (!activations.empty()) {
        attributes_.push_back(NodeAttribute{"activations", activations});
      }
    }
  }

  void SetPadAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kPad);

    const MLPadOperator* pad_op = static_cast<const MLPadOperator*>(op);
    const MLPadOptions* options =
        static_cast<const MLPadOptions*>(pad_op->Options());

    attributes_.push_back(
        NodeAttribute{"beginningPadding", pad_op->BeginningPadding()});

    attributes_.push_back(
        NodeAttribute{"endingPadding", pad_op->EndingPadding()});

    CHECK(options->hasMode());  // hasMode always true.
    attributes_.push_back(NodeAttribute{"mode", options->mode()});

    webnn::MLNumber pad_value = pad_op->Value();
    attributes_.push_back(NodeAttribute{
        "value", MLNumberToString(pad_value, pad_op->Inputs()[0]->DataType())});
  }

  void SetPool2dAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kPool2d);
    const MLPool2dOptions* options =
        static_cast<const MLPool2dOptions*>(op->Options());

    if (options->hasWindowDimensions()) {
      attributes_.push_back(
          NodeAttribute{"windowDimensions", options->windowDimensions()});
    }

    if (options->hasPadding()) {
      attributes_.push_back(NodeAttribute{"padding", options->padding()});
    }
    if (options->hasStrides()) {
      attributes_.push_back(NodeAttribute{"strides", options->strides()});
    }
    if (options->hasDilations()) {
      attributes_.push_back(NodeAttribute{"dilations", options->dilations()});
    }
    if (options->hasLayout()) {
      attributes_.push_back(NodeAttribute{"layout", options->layout()});
    }
    if (options->hasRoundingType()) {
      attributes_.push_back(
          NodeAttribute{"roundingType", options->roundingType()});
    }
    if (options->hasOutputSizes()) {
      attributes_.push_back(
          NodeAttribute{"outputSizes", options->outputSizes()});
    }
  }

  void SetReduceAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kReduce);

    const MLReduceOptions* options =
        static_cast<const MLReduceOptions*>(op->Options());

    if (options->hasAxes()) {
      attributes_.push_back(NodeAttribute{"axes", options->axes()});
    }

    if (options->hasKeepDimensions()) {
      attributes_.push_back(
          NodeAttribute{"keepDimensions", options->keepDimensions()});
    }
  }

  void SetResample2dAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kResample2d);
    const MLResample2dOptions* options =
        static_cast<const MLResample2dOptions*>(op->Options());
    if (options->hasMode()) {
      attributes_.push_back(NodeAttribute{"mode", options->mode()});
    }
    if (options->hasScales()) {
      attributes_.push_back(NodeAttribute{"scales", options->scales()});
    }
    if (options->hasSizes()) {
      attributes_.push_back(NodeAttribute{"sizes", options->sizes()});
    }
    if (options->hasAxes()) {
      attributes_.push_back(NodeAttribute{"axes", options->axes()});
    }
  }

  void SetReverseAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kReverse);
    const MLReverseOptions* options =
        static_cast<const MLReverseOptions*>(op->Options());

    attributes_.push_back(NodeAttribute{"axes", options->axes()});
  }

  void SetScatterElementsAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kScatterElements);
    const MLScatterOptions* options =
        static_cast<const MLScatterOptions*>(op->Options());

    if (options->hasAxis()) {
      attributes_.push_back(NodeAttribute{"axis", options->axis()});
    }
  }

  void SetScatterNdAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kScatterNd);
    const MLScatterOptions* options =
        static_cast<const MLScatterOptions*>(op->Options());

    if (options->hasAxis()) {
      attributes_.push_back(NodeAttribute{"axis", options->axis()});
    }
  }

  void SetSliceAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kSlice);
    const MLSliceOperator* slice_op = static_cast<const MLSliceOperator*>(op);
    const MLSliceOptions* options =
        static_cast<const MLSliceOptions*>(slice_op->Options());

    attributes_.push_back(NodeAttribute{"starts", slice_op->Starts()});

    attributes_.push_back(NodeAttribute{"sizes", slice_op->Sizes()});

    if (options->hasStrides()) {
      attributes_.push_back(NodeAttribute{"strides", options->strides()});
    }
  }

  void SetSoftmaxAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kSoftmax);
    const MLSoftmaxOperator* softmax_op =
        static_cast<const MLSoftmaxOperator*>(op);

    int32_t axis = softmax_op->Axis();
    attributes_.push_back(NodeAttribute{"axis", axis});
  }

  void SetSplitAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kSplit);
    const MLSplitOperator* split_op = static_cast<const MLSplitOperator*>(op);

    const MLSplitOptions* options =
        static_cast<const MLSplitOptions*>(split_op->Options());

    if (split_op->IsEvenSplit()) {
      attributes_.push_back(NodeAttribute{"splits", split_op->SplitNumber()});
    } else {
      attributes_.push_back(NodeAttribute{"splits", split_op->SplitSizes()});
    }

    if (options->hasAxis()) {
      attributes_.push_back(NodeAttribute{"axis", options->axis()});
    }
  }

  void SetTileAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kTile);
    const MLTileOperator* tile_op = static_cast<const MLTileOperator*>(op);

    attributes_.push_back(NodeAttribute{"repetitions", tile_op->Repetitions()});
  }

  void SetTransposeAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kTranspose);
    const MLTransposeOptions* options =
        static_cast<const MLTransposeOptions*>(op->Options());

    if (options->hasPermutation()) {
      attributes_.push_back(
          NodeAttribute{"permutation", options->permutation()});
    }
  }

  void SetTriangularAttributes(const MLOperator* op) {
    CHECK_EQ(op->Kind(), webnn::mojom::blink::Operation::Tag::kTriangular);
    const MLTriangularOptions* options =
        static_cast<const MLTriangularOptions*>(op->Options());

    if (options->hasUpper()) {
      attributes_.push_back(NodeAttribute{"upper", options->upper()});
    }

    if (options->hasDiagonal()) {
      attributes_.push_back(NodeAttribute{"diagonal", options->diagonal()});
    }
  }

  wtf_size_t id_;
  std::string opkind_;

  // The label can be used as the namespace/hierarchy data of the node.
  // https://github.com/google-ai-edge/model-explorer/blob/model-explorer-v0.1.30/src/ui/src/components/visualizer/common/input_graph.ts#L156
  //
  // The namespace/hierarchy data of the node in the form of a "path" (e.g.
  // a/b/c). Don't include the node label as the last component of the
  // namespace. The visualizer will use this data to visualize nodes in a
  // nested way.

  // For example, for three nodes with the following label and namespace data:
  // - N1: a/b
  // - N2: a/b
  // - N3: a

  // The visualizer will first show a collapsed box labeled 'a'. After the box
  // is expanded (by user clicking on it), it will show node N3, and another
  // collapsed box labeled 'b'. After the box 'b' is expanded, it will show
  // two nodes N1 and N2 inside the box 'b'.

  std::string label_;

  Vector<IncomingEdge> incoming_edges_;
  Vector<NodeAttribute> attributes_;
  Vector<Vector<uint32_t>> output_shapes_;
  Vector<V8MLOperandDataType> output_data_types_;
};

}  // namespace

void MLGraphDumper::NodeIdMapper::Trace(Visitor* visitor) const {
  visitor->Trace(op_to_id_map_);
  visitor->Trace(input_constant_operand_to_id_map_);
}

wtf_size_t MLGraphDumper::NodeIdMapper::NextId(Member<const MLOperator> op) {
  if (!op_to_id_map_.Contains(op)) {
    wtf_size_t new_id = NextNewId();
    op_to_id_map_.Set(op, new_id);
    return new_id;
  }
  return op_to_id_map_.at(op);
}

wtf_size_t MLGraphDumper::NodeIdMapper::NextId(
    Member<const MLOperand> operand) {
  if (!input_constant_operand_to_id_map_.Contains(operand)) {
    wtf_size_t new_id = NextNewId();
    input_constant_operand_to_id_map_.Set(operand, new_id);
    return new_id;
  }
  return input_constant_operand_to_id_map_.at(operand);
}

wtf_size_t MLGraphDumper::NodeIdMapper::NextId(
    const String& graph_output_name) {
  if (!graph_output_name_to_id_map_.Contains(graph_output_name)) {
    wtf_size_t new_id = NextNewId();
    graph_output_name_to_id_map_.Set(graph_output_name, new_id);
    return new_id;
  }
  return graph_output_name_to_id_map_.at(graph_output_name);
}

wtf_size_t MLGraphDumper::NodeIdMapper::NextNewId() {
  return op_to_id_map_.size() + input_constant_operand_to_id_map_.size() +
         graph_output_name_to_id_map_.size();
}

MLGraphDumper::MLGraphDumper() {
  node_id_mapper_ = MakeGarbageCollected<NodeIdMapper>();

  std::string collection_id = base::UnlocalizedTimeFormatWithPattern(
      base::Time::Now(), "'webnn_graph_'yyyyMMdd-HHmmss");

  root_.Set("label", collection_id);
  root_.Set("graphs", base::Value::List());
  root_.Set("graphSorting", "name_asc");
}

void MLGraphDumper::Trace(Visitor* visitor) const {
  visitor->Trace(node_id_mapper_);
}

void MLGraphDumper::RecordGraph(const std::string& graph_id,
                                const MLNamedOperands& named_outputs) {
  HeapVector<Member<MLOperator>> ops =
      GetOperatorsInTopologicalOrder(named_outputs);

  HeapHashMap<Member<MLOperator>, Member<Node>> op_to_node_map;
  HeapHashMap<Member<MLOperand>, Member<Node>>
      input_or_constant_operand_to_node_map;

  HeapVector<Member<Node>> nodes;

  for (auto& op : ops) {
    MLOperatorOptions* options = op->Options();
    std::string op_label = options ? options->label().Utf8() : "";

    Node* node = MakeGarbageCollected<Node>(
        node_id_mapper_->NextId(op),
        MLOperator::OperatorKindToString(op->Kind(), op->SubKind()).Utf8(),
        op_label, GetShapesOfOperatorOutputPorts(op),
        GetDataTypesOfOperatorOutputPorts(op));

    node->SetOpAttributes(op);
    nodes.push_back(node);
    op_to_node_map.Set(op, node);

    for (wtf_size_t input_idx = 0; input_idx < op->Inputs().size();
         ++input_idx) {
      const auto& input_operand = op->Inputs()[input_idx];
      if (input_operand->Kind() == webnn::mojom::Operand_Kind::kInput ||
          input_operand->Kind() == webnn::mojom::Operand_Kind::kConstant) {
        Node* input = nullptr;
        std::string opkind =
            input_operand->Kind() == webnn::mojom::Operand_Kind::kInput
                ? "Input"
                : "Constant";
        if (!input_or_constant_operand_to_node_map.Contains(input_operand)) {
          input = MakeGarbageCollected<Node>(
              node_id_mapper_->NextId(input_operand), opkind, "",
              Vector<Vector<uint32_t>>{input_operand->shape()},
              Vector<V8MLOperandDataType>{input_operand->dataType()});
          nodes.push_back(input);
          input_or_constant_operand_to_node_map.Set(input_operand, input);
        } else {
          input = input_or_constant_operand_to_node_map.at(input_operand);
        }
        // Source node is a input node, it only has itself as the output so set
        // the output_id to 0
        IncomingEdge edge{/*source_id=*/input->Id(),
                          /*source_output_id=*/0,
                          /*target_input_id=*/input_idx};
        node->AppendInputEdge(edge);

      } else {
        MLOperator* source_op = input_operand->Operator();
        Node* source_node = op_to_node_map.at(source_op);
        IncomingEdge edge{/*source_id=*/source_node->Id(),
                          /*source_output_id=*/
                          GetOutputIndex(source_op, input_operand),
                          /*target_input_id=*/input_idx};
        node->AppendInputEdge(edge);
      }
    }
  }

  for (auto& named_output : named_outputs) {
    const String& output_name = named_output.first;
    MLOperand* output_operand = named_output.second;
    MLOperator* output_operator = output_operand->Operator();

    Node* output = MakeGarbageCollected<Node>(
        node_id_mapper_->NextId(output_name), "Output", "",
        Vector<Vector<uint32_t>>{output_operand->shape()},
        Vector<V8MLOperandDataType>{output_operand->dataType()});
    output->SetAttribute("output_name", output_name);
    nodes.push_back(output);
    auto* source_node = op_to_node_map.at(output_operator);
    IncomingEdge edge{/*source_id=*/source_node->Id(),
                      /*source_output_id=*/
                      GetOutputIndex(output_operator, output_operand),
                      /*target_input_id=*/0};
    output->AppendInputEdge(edge);
  }

  base::Value::Dict graph_json;
  graph_json.Set("id", graph_id);
  base::Value::List nodes_json;

  for (auto& node : nodes) {
    nodes_json.Append(node->ToJson());
  }
  graph_json.Set("nodes", std::move(nodes_json));

  base::Value::List* graphs = root_.FindList("graphs");
  if (graphs) {
    graphs->Append(std::move(graph_json));
  }
}

}  // namespace blink
