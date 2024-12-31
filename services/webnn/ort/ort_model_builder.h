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
#include "services/webnn/ort/allocator_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

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

    ScopedOrtModel model;

    // TODO: Consider reusing constant operands instead of copying them to
    // `external_data`.
    //
    // Store the external data which should be alive for inference session.
    std::vector<base::HeapArray<uint8_t>> external_data;
  };

  explicit OrtModelBuilder(scoped_refptr<AllocatorOrt> allocator);
  ~OrtModelBuilder();
  OrtModelBuilder(const OrtModelBuilder&) = delete;
  OrtModelBuilder& operator=(const OrtModelBuilder&) = delete;

  void AddInput(std::string_view name,
                base::span<const int64_t> shape,
                ONNXTensorElementDataType data_type);

  void AddOutput(std::string_view name,
                 base::span<const int64_t> shape,
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
  void CreateAttribute(ScopedOrtOpAttr& attribute,
                       std::string_view name,
                       OrtOpAttrData data);

  void AddNode(std::string_view op_type,
               std::string_view node_name,
               base::span<const char*> input_names,
               base::span<const char*> output_names,
               base::span<OrtOpAttr**> attributes = {});

  std::unique_ptr<ModelInfo> BuildAndTakeModelInfo();

 private:
  scoped_refptr<AllocatorOrt> allocator_;

  ScopedOrtGraph graph_;

  std::unique_ptr<ModelInfo> model_info_;
};

}  // namespace ort
}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_ORT_MODEL_BUILDER_H_
