// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_ie.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_delegate_ie.h"
#include "third_party/libinference_engine/dldt/inference-engine/include/inference_engine.hpp"

namespace ie = InferenceEngine;

namespace ml {

ExecutionImplIe::ExecutionImplIe(const CompilationDelegateIe* compilation,
                                 mojom::ExecutionInitParamsPtr params)
    : compilation_(compilation),
      params_(std::move(params)),
      infer_request_(nullptr),
      plugin_(nullptr),
      execution_(nullptr) {}

int32_t ExecutionImplIe::Init(int32_t preference) {
  try {
    std::string device_name;
    if (preference == mojom::PREFER_FAST_SINGLE_ANSWER) {
      device_name = "CPU";
    } else if (preference == mojom::PREFER_SUSTAINED_SPEED) {
      device_name = "GPU";
    } else if (preference == mojom::PREFER_LOW_POWER) {
      device_name = "VPU";
    }
    DLOG(INFO) << "[IE] Trying to get plugin by device name " << device_name;
    plugin_.reset(
        new ie::InferencePlugin(static_cast<ie::InferenceEnginePluginPtr>(
            ie::PluginDispatcher({""}).getPluginByDevice(device_name))));
    const ie::Version* version = plugin_->GetVersion();
    DLOG(INFO) << "[IE] succeed to load plugin " << version->buildNumber << " "
               << version->description;
    execution_.reset(
        new ie::ExecutableNetwork(static_cast<ie::IExecutableNetwork::Ptr&>(
            plugin_->LoadNetwork(*(compilation_->network_), {}))));
    DLOG(INFO) << "[IE] succeed to load network to plugin";
    infer_request_.reset(new ie::InferRequest(
        static_cast<ie::IInferRequest::Ptr>(execution_->CreateInferRequest())));
    initialized_ = true;
  } catch (const std::exception& ex) {
    LOG(ERROR) << "[IE] exception " << ex.what();
    initialized_ = false;
    return mojom::OP_FAILED;
  }
  return mojom::NOT_ERROR;
}

ExecutionImplIe::~ExecutionImplIe() {
  DLOG(INFO) << "ExecutionImplIe::~ExecutionImplIe()";
  // Release in squence to avoid crash.
  infer_request_.reset(nullptr);
  execution_.reset(nullptr);
  plugin_.reset(nullptr);
}

void ExecutionImplIe::StartCompute(StartComputeCallback callback) {
  DLOG(INFO) << "ExecutionImplIe::StartCompute";
  if (!initialized_) {
    LOG(ERROR) << "Not initialized";
    std::move(callback).Run(mojom::BAD_STATE);
    return;
  }
  try {
    int32_t result;
    uint32_t total_length = 0;
    for (size_t i = 0; i < params_->inputs.size(); ++i) {
      const mojom::OperandInfoPtr& operand = params_->inputs[i];
      const uint32_t offset = total_length;
      const uint32_t length = GetRequiredSize(operand);
      total_length += length;
      if (operand->type != mojom::TENSOR_FLOAT32) {
        LOG(ERROR) << "Only TENSOR_FLOAT32 operand type is supported";
        std::move(callback).Run(mojom::BAD_DATA);
        return;
      }
      auto mapping = params_->memory->MapAtOffset(length, offset);
      DLOG(INFO) << "Mapping " << mapping.get() << " for input " << i
                 << " offset " << offset << " length " << length;
      std::string input_id = base::NumberToString(operand->index);
      ie::Blob::Ptr input_blob = infer_request_->GetBlob(input_id);
      float* dst =
          input_blob->buffer()
              .as<ie::PrecisionTrait<ie::Precision::FP32>::value_type*>();
      const float* src = reinterpret_cast<const float*>(mapping.get());
      if (operand->dimensions.size() == 3) {
        // Only reorder HWC to CHW
        result = CompilationDelegateIe::Reorder(dst, src, operand->dimensions);
        if (result != mojom::NOT_ERROR) {
          std::move(callback).Run(mojom::BAD_DATA);
          return;
        }
      } else {
        const size_t length = product(operand->dimensions) * sizeof(float);
        memcpy(static_cast<void*>(dst), static_cast<const void*>(src), length);
      }
      DLOG(INFO) << "Copy data to input blob buffer for " << input_id;
    }

    infer_request_->Infer();

    for (size_t i = 0; i < params_->outputs.size(); ++i) {
      const mojom::OperandInfoPtr& operand = params_->outputs[i];
      const uint32_t offset = total_length;
      const uint32_t length = GetRequiredSize(operand);
      total_length += length;
      auto mapping = params_->memory->MapAtOffset(length, offset);
      DLOG(INFO) << "Mapping " << mapping.get() << " for output " << i
                 << " offset " << offset << " length " << length;
      std::string output_id = base::NumberToString(operand->index);
      const ie::Blob::Ptr output_blob = infer_request_->GetBlob(output_id);
      const float* src =
          output_blob->buffer()
              .as<ie::PrecisionTrait<ie::Precision::FP32>::value_type*>();
      float* dst = reinterpret_cast<float*>(mapping.get());
      if (operand->dimensions.size() == 3) {
        result = CompilationDelegateIe::Reorder(dst, src, operand->dimensions,
                                                false);
        if (result != mojom::NOT_ERROR) {
          std::move(callback).Run(mojom::BAD_DATA);
          return;
        }
      } else {
        const size_t length = product(operand->dimensions) * sizeof(float);
        memcpy(static_cast<void*>(dst), static_cast<const void*>(src), length);
      }
      DLOG(INFO) << "Copy data from output memory primitive buffer for "
                 << output_id;
    }
  } catch (const std::exception& ex) {
    LOG(ERROR) << "[IE] exception " << ex.what();
    std::move(callback).Run(mojom::OP_FAILED);
  }
  DLOG(INFO) << "ExecutionImplIe::StartCompute ends.";
  std::move(callback).Run(mojom::NOT_ERROR);
}

}  // namespace ml
