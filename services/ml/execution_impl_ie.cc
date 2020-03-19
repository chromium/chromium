// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/execution_impl_ie.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "services/ml/compilation_delegate_ie.h"
#include "third_party/libinference_engine/dldt/inference-engine/include/gna/gna_config.hpp"
#include "third_party/libinference_engine/dldt/inference-engine/include/inference_engine.hpp"

namespace ie = InferenceEngine;

namespace ml {

// TODO(Junwei): GNA device only be opened for one instance of
// ExecutableNetwork, there will be memory leak for these static objects.
static std::unique_ptr<InferenceEngine::InferencePlugin> s_gna_plugin = nullptr;
static std::unique_ptr<InferenceEngine::ExecutableNetwork> s_gna_execution =
    nullptr;
static std::unique_ptr<InferenceEngine::InferRequest> s_gna_infer_request =
    nullptr;

ExecutionImplIe::ExecutionImplIe(const CompilationDelegateIe* compilation,
                                 mojom::ExecutionInitParamsPtr params,
                                 float input_scale)
    : compilation_(compilation),
      params_(std::move(params)),
      input_scale_(input_scale),
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
      device_name = "MYRIAD";
    } else if (preference == mojom::PREFER_ULTRA_LOW_POWER) {
      device_name = "GNA";
      // Release in squence to avoid crash. Close GNA device befere re-open,
      s_gna_infer_request.reset(nullptr);
      s_gna_execution.reset(nullptr);
      s_gna_plugin.reset(nullptr);
    }
    DLOG(INFO) << "[IE] Trying to get plugin by device name " << device_name;
    std::unique_ptr<InferenceEngine::InferRequest> infer_request;
    std::unique_ptr<InferenceEngine::InferencePlugin> plugin;
    std::unique_ptr<InferenceEngine::ExecutableNetwork> execution;
    plugin.reset(
        new ie::InferencePlugin(static_cast<ie::InferenceEnginePluginPtr>(
            ie::PluginDispatcher({
#if defined(OS_WIN)
              L""  // Windows support UNICODE.
#else
              ""
#endif
            }).getPluginByDevice(device_name))));
    const ie::Version* version = plugin->GetVersion();
    DLOG(INFO) << "[IE] succeed to load plugin " << version->buildNumber << " "
               << version->description;
    std::map<std::string, std::string> plugin_Config = {};
    if (preference == mojom::PREFER_ULTRA_LOW_POWER && input_scale_ > 0) {
      std::string scaleFactorConfigKey = GNA_CONFIG_KEY(SCALE_FACTOR);
      plugin_Config[scaleFactorConfigKey] = std::to_string(input_scale_);
      // Note that it is not always possible to use 8-bit weights due to GNA
      // hardware limitations. For example, convolutional layers always use
      // 16-bit weights (GNA harware verison 1 and 2). This limitation will be
      // removed in GNA hardware version 3 and higher.
      // gnaPluginConfig[ie::GNAConfigParams::KEY_GNA_PRECISION] = "I8";
    }
    execution.reset(
        new ie::ExecutableNetwork(static_cast<ie::IExecutableNetwork::Ptr&>(
            plugin->LoadNetwork(*(compilation_->network_), plugin_Config))));
    DLOG(INFO) << "[IE] succeed to load network to plugin";
    infer_request.reset(new ie::InferRequest(
        static_cast<ie::IInferRequest::Ptr>(execution->CreateInferRequest())));
    initialized_ = true;
    preference_ = preference;
    if (preference == mojom::PREFER_ULTRA_LOW_POWER) {
      s_gna_infer_request = std::move(infer_request);
      s_gna_execution = std::move(execution);
      s_gna_plugin = std::move(plugin);
    } else {
      infer_request_ = std::move(infer_request);
      execution_ = std::move(execution);
      plugin_ = std::move(plugin);
    }
  } catch (const std::exception& ex) {
    LOG(ERROR) << "[IE] exception " << ex.what();
    initialized_ = false;
    return mojom::OP_FAILED;
  }
  return mojom::NOT_ERROR;
}

ExecutionImplIe::~ExecutionImplIe() {
  DLOG(INFO) << "ExecutionImplIe::~ExecutionImplIe()";
  if (preference_ != mojom::PREFER_ULTRA_LOW_POWER) {
    // Release in squence to avoid crash.
    infer_request_.reset(nullptr);
    execution_.reset(nullptr);
    plugin_.reset(nullptr);
  }
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
    InferenceEngine::InferRequest* infer_request =
        preference_ == mojom::PREFER_ULTRA_LOW_POWER ? s_gna_infer_request.get()
                                                     : infer_request_.get();
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
      ie::Blob::Ptr input_blob = infer_request->GetBlob(input_id);
      float* dst =
          input_blob->buffer()
              .as<ie::PrecisionTrait<ie::Precision::FP32>::value_type*>();
      const float* src = reinterpret_cast<const float*>(mapping.get());
      if (operand->dimensions.size() == 3) {
        // Only reorder HWC to CHW
        result = CompilationDelegateIe::Reorder<float>(dst, src,
                                                       operand->dimensions);
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

    infer_request->Infer();

    for (size_t i = 0; i < params_->outputs.size(); ++i) {
      const mojom::OperandInfoPtr& operand = params_->outputs[i];
      const uint32_t offset = total_length;
      const uint32_t length = GetRequiredSize(operand);
      total_length += length;
      auto mapping = params_->memory->MapAtOffset(length, offset);
      DLOG(INFO) << "Mapping " << mapping.get() << " for output " << i
                 << " offset " << offset << " length " << length;
      std::string output_id = base::NumberToString(operand->index);
      const ie::Blob::Ptr output_blob = infer_request->GetBlob(output_id);
      const float* src =
          output_blob->buffer()
              .as<ie::PrecisionTrait<ie::Precision::FP32>::value_type*>();
      float* dst = reinterpret_cast<float*>(mapping.get());
      if (operand->dimensions.size() == 3) {
        result = CompilationDelegateIe::Reorder<float>(
            dst, src, operand->dimensions, false);
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
