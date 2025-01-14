// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ORT_MODEL_BUILDER_H_
#define SERVICES_WEBNN_ORT_ORT_MODEL_BUILDER_H_

#include <memory>
#include <string>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn {

namespace ort {

class OrtModelBuilder final {
  STACK_ALLOCATED();

 public:
  struct ModelInfo {
    explicit ModelInfo();
    ModelInfo(const ModelInfo&) = delete;
    ModelInfo& operator=(const ModelInfo&) = delete;
    ~ModelInfo();

    ScopedOrtModelPtr model;

    // TODO(https://github.com/shiyi9801/chromium/issues/49): Consider reusing
    // constant operands instead of copying them to `external_data`.
    //
    // Store the external data which should be alive for inference session.
    std::vector<base::HeapArray<uint8_t>> external_data;
  };

  OrtModelBuilder();
  ~OrtModelBuilder();
  OrtModelBuilder(const OrtModelBuilder&) = delete;
  OrtModelBuilder& operator=(const OrtModelBuilder&) = delete;

  void AddInput(std::string_view name,
                base::span<const int64_t> shape,
                ONNXTensorElementDataType data_type);

  void AddOutput(std::string_view name,
                 base::span<const int64_t> shape,
                 ONNXTensorElementDataType data_type);

  // Add an initializer to the graph. It will use raw data if byte size
  // of the data is less than 128.
  void AddInitializer(std::string_view name,
                      base::span<const int64_t> shape,
                      base::span<const uint8_t> data,
                      ONNXTensorElementDataType data_type);

  void AddInitializerAsRawData(std::string_view name,
                               base::span<const int64_t> shape,
                               base::span<const uint8_t> data,
                               ONNXTensorElementDataType data_type);

  void AddInitializerAsExternalData(std::string_view name,
                                    base::span<const int64_t> shape,
                                    base::span<const uint8_t> data,
                                    ONNXTensorElementDataType data_type);

  using OrtOpAttrData = absl::variant<int64_t,
                                      float,
                                      std::string_view,
                                      base::span<const int64_t>,
                                      base::span<const float>,
                                      base::span<const char*>>;
  ScopedOrtOpAttrPtr CreateAttribute(std::string_view name, OrtOpAttrData data);

  // Node will own attributes.
  void AddNode(std::string_view op_type,
               std::string_view node_name,
               base::span<const char*> input_names,
               base::span<const char*> output_names,
               base::span<OrtOpAttr*> attributes = {});

  std::unique_ptr<ModelInfo> BuildAndTakeModelInfo();

 private:
  std::vector<ScopedOrtValueInfoPtr> inputs_;
  std::vector<ScopedOrtValueInfoPtr> outputs_;

  ScopedOrtMemoryInfoPtr memory_info_;
  ScopedOrtGraphPtr graph_;

  std::unique_ptr<ModelInfo> model_info_;
};

}  // namespace ort
}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_ORT_MODEL_BUILDER_H_
