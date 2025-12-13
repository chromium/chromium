// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_MODEL_EDITOR_H_
#define SERVICES_WEBNN_ORT_MODEL_EDITOR_H_

#include <memory>
#include <variant>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/strings/cstring_view.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"

namespace webnn::ort {

class ExternalWeightsManager;

class COMPONENT_EXPORT(WEBNN_SERVICE) ModelEditor {
 public:
  struct COMPONENT_EXPORT(WEBNN_SERVICE) ModelInfo {
    ModelInfo();
    ModelInfo(const ModelInfo&) = delete;
    ModelInfo& operator=(const ModelInfo&) = delete;
    ~ModelInfo();

    // `external_weights_manager` should be prior to `model` since
    // `external_weights_manager` will be called by ORT to release the external
    // weights during `model` destruction.
    std::unique_ptr<ExternalWeightsManager> external_weights_manager;

    ScopedOrtModel model;

    base::flat_map<std::string, std::string>
        operand_input_name_to_onnx_input_name;
    base::flat_map<std::string, std::string>
        operand_output_name_to_onnx_output_name;
  };

  ModelEditor();
  ~ModelEditor();
  ModelEditor(const ModelEditor&) = delete;
  ModelEditor& operator=(const ModelEditor&) = delete;

  void AddInput(base::cstring_view name, const mojom::Operand& input);

  void AddOutput(base::cstring_view name, const mojom::Operand& output);

  void AddInitializer(base::cstring_view name,
                      std::unique_ptr<WebNNConstantOperand> constant_operand);

  // Add an initializer directly into the ONNX model.
  // This method could be useful for converting a WebNN operator's attribute to
  // an ONNX model initializer. For example, the reshape operator's new shape is
  // a tensor/initializer input in ONNX spec.
  void AddInitializer(base::cstring_view name,
                      ONNXTensorElementDataType data_type,
                      base::span<const int64_t> shape,
                      base::span<const uint8_t> data);

  using OrtOpAttrData = std::variant<int64_t,
                                     float,
                                     base::cstring_view,
                                     base::span<const int64_t>,
                                     base::span<const float>,
                                     base::span<const char*>>;
  ScopedOrtOpAttr CreateAttribute(base::cstring_view name, OrtOpAttrData data);

  // The ownership of `attributes` will be transferred and should not be used
  // after this call.
  void AddNode(base::cstring_view op_type,
               base::cstring_view node_name,
               base::span<const char*> inputs,
               base::span<const char*> outputs,
               base::span<ScopedOrtOpAttr> attributes = {});

  // No further methods should be called on this class after calling this
  // method.
  std::unique_ptr<ModelInfo> BuildAndTakeModelInfo();

 private:
  // Add an initializer and copy the data into the graph.
  void AddInitializerAsRawData(base::cstring_view name,
                               ONNXTensorElementDataType data_type,
                               base::span<const int64_t> shape,
                               base::span<const uint8_t> data);

  // Add an initializer and transfer the data into
  // `model_info_->external_weights_manager`.
  void AddInitializerAsExternalData(base::cstring_view name,
                                    ONNXTensorElementDataType data_type,
                                    base::span<const int64_t> shape,
                                    base::HeapArray<uint8_t> data);

  std::vector<ScopedOrtValueInfo> inputs_;
  std::vector<ScopedOrtValueInfo> outputs_;

  ScopedOrtGraph graph_;

  std::unique_ptr<ModelInfo> model_info_;

  bool has_built_ = false;

  std::vector<std::pair<std::string, std::string>>
      operand_input_name_to_onnx_input_name_map;
  std::vector<std::pair<std::string, std::string>>
      operand_output_name_to_onnx_output_name_map;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_MODEL_EDITOR_H_
