// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include <math.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "services/webnn/error.h"
#include "services/webnn/gpu_task_scheduler.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    base::flat_map<std::string, OperandDescriptor> input_names_to_descriptors,
    base::flat_map<std::string, OperandDescriptor> output_names_to_descriptors,
    base::flat_map<OperandId, base::flat_set<OperationId>>
        operand_to_dependent_operations,
    base::flat_map<OperandId, OperationId> operand_to_producing_operation,
    base::PassKey<WebNNGraphBuilderImpl> pass_key)
    : input_names_to_descriptors(std::move(input_names_to_descriptors)),
      output_names_to_descriptors(std::move(output_names_to_descriptors)),
      operand_to_dependent_operations(
          std::move(operand_to_dependent_operations)),
      operand_to_producing_operation(
          std::move(operand_to_producing_operation)) {}

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    base::flat_map<std::string, OperandDescriptor> input_names_to_descriptors,
    base::flat_map<std::string, OperandDescriptor> output_names_to_descriptors,
    base::PassKey<ort::GraphImplOrt> pass_key)
    : input_names_to_descriptors(std::move(input_names_to_descriptors)),
      output_names_to_descriptors(std::move(output_names_to_descriptors)) {}

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    base::flat_map<std::string, OperandDescriptor> input_names_to_descriptors,
    base::flat_map<std::string, OperandDescriptor> output_names_to_descriptors,
    base::PassKey<coreml::GraphImplCoreml> pass_key)
    : input_names_to_descriptors(std::move(input_names_to_descriptors)),
      output_names_to_descriptors(std::move(output_names_to_descriptors)) {}

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    ComputeResourceInfo&&) = default;
WebNNGraphImpl::ComputeResourceInfo&
WebNNGraphImpl::ComputeResourceInfo::operator=(ComputeResourceInfo&&) = default;

WebNNGraphImpl::ComputeResourceInfo::~ComputeResourceInfo() = default;

namespace {

std::string_view GetDeviceTypeHistogramSuffix(mojom::Device device) {
  switch (device) {
    case mojom::Device::kCpu:
      return "CPU";
    case mojom::Device::kGpu:
      return "GPU";
    case mojom::Device::kNpu:
      return "NPU";
  }
}

void RecordGraphDevices(mojom::Device context_device,
                        base::span<const mojom::Device> graph_devices) {
  // Backends that do not report per-graph execution devices pass an empty list.
  if (graph_devices.empty()) {
    return;
  }

  bool has_cpu = false;
  bool has_gpu = false;
  bool has_npu = false;
  for (mojom::Device device : graph_devices) {
    switch (device) {
      case mojom::Device::kCpu:
        has_cpu = true;
        break;
      case mojom::Device::kGpu:
        has_gpu = true;
        break;
      case mojom::Device::kNpu:
        has_npu = true;
        break;
    }
  }

  WebNNGraphImpl::GraphDevicesUma uma_value;
  if (has_cpu && has_gpu && has_npu) {
    uma_value = WebNNGraphImpl::GraphDevicesUma::kCpuGpuAndNpu;
  } else if (has_gpu && has_npu) {
    uma_value = WebNNGraphImpl::GraphDevicesUma::kGpuAndNpu;
  } else if (has_cpu && has_npu) {
    uma_value = WebNNGraphImpl::GraphDevicesUma::kCpuAndNpu;
  } else if (has_cpu && has_gpu) {
    uma_value = WebNNGraphImpl::GraphDevicesUma::kCpuAndGpu;
  } else if (has_npu) {
    uma_value = WebNNGraphImpl::GraphDevicesUma::kNpuOnly;
  } else if (has_gpu) {
    uma_value = WebNNGraphImpl::GraphDevicesUma::kGpuOnly;
  } else {
    CHECK(has_cpu);
    uma_value = WebNNGraphImpl::GraphDevicesUma::kCpuOnly;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"WebNN.Graph.Devices.",
                    GetDeviceTypeHistogramSuffix(context_device)}),
      uma_value);
}

}  // namespace

WebNNGraphImpl::WebNNGraphImpl(WebNNContextImpl& context,
                               ComputeResourceInfo compute_resource_info,
                               std::vector<mojom::Device> devices)
    : base::RefCountedDeleteOnSequence<WebNNGraphImpl>(
          context.owning_task_runner()),
      context_(context),
      compute_resource_info_(std::move(compute_resource_info)),
      devices_(std::move(devices)) {
  RecordGraphDevices(context.options().device, devices_);
}

WebNNGraphImpl::~WebNNGraphImpl() = default;

void WebNNGraphImpl::RunDispatch(
    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        name_to_input_tensor_map,
    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
        name_to_output_tensor_map,
    ScopedTrace scoped_trace,
    mojo::ReportBadMessageCallback bad_message_cb) {
  context_->RunOrScheduleTask(base::BindOnce(
      [](WebNNGraphImpl* self,
         base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
             name_to_input_tensor_map,
         base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
             name_to_output_tensor_map,
         ScopedTrace scoped_trace,
         mojo::ReportBadMessageCallback bad_message_cb) {
        for (auto& [name, tensor] : name_to_input_tensor_map) {
          if (tensor->is_exported()) {
            LOG(ERROR)
                << "[WebNN] Invalid to dispatch graph when input tensor (" +
                       name + ") is exported.";
            std::move(bad_message_cb).Run(kBadMessageInvalidTensor);
            return;
          }
        }

        for (auto& [name, tensor] : name_to_output_tensor_map) {
          if (tensor->is_exported()) {
            LOG(ERROR) << "[WebNN] Invalid to dispatch graph when output "
                          "tensor (" +
                              name + ") is exported.";
            std::move(bad_message_cb).Run(kBadMessageInvalidTensor);
            return;
          }
        }

        self->DispatchImpl(std::move(name_to_input_tensor_map),
                           std::move(name_to_output_tensor_map));
      },
      base::RetainedRef(this), std::move(name_to_input_tensor_map),
      std::move(name_to_output_tensor_map), std::move(scoped_trace),
      std::move(bad_message_cb)));
}

}  // namespace webnn
