// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ORT_MODEL_EDITOR_H_
#define SERVICES_WEBNN_ORT_ORT_MODEL_EDITOR_H_

#include <memory>
#include <string>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn {

namespace ort {

// Domains
constexpr char kOrtDomainName[] = "";
constexpr char kMSDomainName[] = "com.microsoft";
// DML fused operators.
constexpr char kMSDmlDomainName[] = "com.microsoft.dml";
// WebGPU EP needs NHWC layout.
constexpr char kMSInternalNhwcDomain[] = "com.ms.internal.nhwc";

// Opsets
constexpr int32_t kOrtOpsetVersion = 21;
// EPContext op is used for exporting the EP context cache model.
// https://onnxruntime.ai/docs/execution-providers/EP-Context-Design.html#onnxruntime-ep-context-cache-feature-design
constexpr int32_t kEPContextOpsetVersion = 1;
constexpr int32_t kMSDmlDomainOpsetVersion = 1;
constexpr int32_t kMSInternalNhwcDomainOpsetVersion = 1;

// Define the minimum size(in bytes) to use external data.
constexpr size_t kMinExternalDataSize = 128;

class OrtModelEditor {
 public:
  struct ModelInfo {
    ModelInfo();
    ModelInfo(const ModelInfo&) = delete;
    ModelInfo& operator=(const ModelInfo&) = delete;
    ~ModelInfo();

    ScopedOrtModel model;

    // TODO(https://github.com/shiyi9801/chromium/issues/49): Consider reusing
    // constant operands instead of copying them to `external_data`.
    //
    // Store the external data which should be alive for inference session.
    std::vector<base::HeapArray<uint8_t>> external_data;
  };

  OrtModelEditor();
  ~OrtModelEditor();
  OrtModelEditor(const OrtModelEditor&) = delete;
  OrtModelEditor& operator=(const OrtModelEditor&) = delete;

  void AddInput(std::string_view name,
                base::span<const int64_t> shape,
                ONNXTensorElementDataType data_type);

  void AddOutput(std::string_view name,
                 base::span<const int64_t> shape,
                 ONNXTensorElementDataType data_type);

  // Add an initializer to the graph. It will use raw data if byte size
  // of the data is less than 128.
  [[nodiscard]] ScopedOrtStatus AddInitializer(
      std::string_view name,
      base::span<const int64_t> shape,
      base::span<const uint8_t> data,
      ONNXTensorElementDataType data_type);

  [[nodiscard]] ScopedOrtStatus AddInitializerAsRawData(
      std::string_view name,
      base::span<const int64_t> shape,
      base::span<const uint8_t> data,
      ONNXTensorElementDataType data_type);

  [[nodiscard]] ScopedOrtStatus AddInitializerAsExternalData(
      std::string_view name,
      base::span<const int64_t> shape,
      base::span<const uint8_t> data,
      ONNXTensorElementDataType data_type);

  using OrtOpAttrData = absl::variant<int64_t,
                                      float,
                                      std::string_view,
                                      base::span<const int64_t>,
                                      base::span<const float>,
                                      base::span<const char*>>;
  ScopedOrtOpAttr CreateAttribute(std::string_view name, OrtOpAttrData data);

  void AddNode(std::string_view op_type,
               std::string_view node_name,
               base::span<const char*> input,
               base::span<const char*> output,
               std::vector<ScopedOrtOpAttr> attributes = {},
               std::string_view domain_name = kOrtDomainName);

  std::unique_ptr<ModelInfo> BuildAndTakeModelInfo();

 private:
  std::vector<ScopedOrtValueInfo> inputs_;
  std::vector<ScopedOrtValueInfo> outputs_;

  ScopedOrtMemoryInfo memory_info_;
  ScopedOrtGraph graph_;

  std::unique_ptr<ModelInfo> model_info_;
};

}  // namespace ort
}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_ORT_MODEL_EDITOR_H_
