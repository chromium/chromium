// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include <math.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/task/bind_post_task.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

namespace {

// Return false if the named tensors for dispatch don't match the built
// graph's expectation.
bool ValidateWebNNTensors(
    const base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>&
        named_tensors,
    const base::flat_map<std::string, OperandDescriptor>&
        names_to_descriptors) {
  return std::ranges::equal(
      named_tensors, names_to_descriptors,
      [](const auto& named_tensor, const auto& tensor_spec) {
        const auto& [tensor_name, tensor_impl] = named_tensor;
        const auto& [tensor_spec_name, tensor_spec_descriptor] = tensor_spec;
        return tensor_name == tensor_spec_name &&
               tensor_impl->data_type() == tensor_spec_descriptor.data_type() &&
               tensor_impl->shape() == tensor_spec_descriptor.shape();
      });
}

// Return false if the same tensor was specified in inputs and outputs.
bool ValidateWebNNTensorsUsage(
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_inputs,
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_outputs) {
  // Validate that output tensors are unique.
  std::set<blink::WebNNTensorToken> output_tensors;
  for (const auto& named_output : named_outputs) {
    output_tensors.insert(named_output.second);
  }

  if (output_tensors.size() != named_outputs.size()) {
    return false;
  }

  // Validate tensors used for input and output are unique.
  for (const auto& named_input : named_inputs) {
    if (output_tensors.contains(named_input.second)) {
      return false;
    }
  }

  return true;
}

}  // namespace

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
    ComputeResourceInfo&&) = default;
WebNNGraphImpl::ComputeResourceInfo&
WebNNGraphImpl::ComputeResourceInfo::operator=(ComputeResourceInfo&&) = default;

WebNNGraphImpl::ComputeResourceInfo::~ComputeResourceInfo() = default;

WebNNGraphImpl::WebNNGraphImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    ComputeResourceInfo compute_resource_info,
    std::vector<mojom::Device> devices)
    : WebNNObjectImpl<mojom::WebNNGraph,
                      blink::WebNNGraphToken,
                      mojo::AssociatedReceiver<mojom::WebNNGraph>>(
          std::move(receiver),
          context->scheduler_task_runner(),
          context->owning_task_runner()),
      context_(std::move(context)),
      compute_resource_info_(std::move(compute_resource_info)),
      devices_(std::move(devices)) {
  CHECK(context_);
}

WebNNGraphImpl::~WebNNGraphImpl() = default;

void WebNNGraphImpl::OnDisconnect() {
  context_->RemoveWebNNGraphImpl(handle());
}

void WebNNGraphImpl::Dispatch(
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_inputs,
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_outputs) {
  ScopedTrace scoped_trace("WebNNGraphImpl::Dispatch");

  if (!ValidateWebNNTensorsUsage(named_inputs, named_outputs)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Resolve the token of a input MLTensor to the corresponding `WebNNTensor`
  // instance.
  std::vector<std::pair<std::string, scoped_refptr<WebNNTensorImpl>>>
      name_to_input_tensors;
  name_to_input_tensors.reserve(named_inputs.size());
  for (const auto& [name, tensor_handle] : named_inputs) {
    scoped_refptr<WebNNTensorImpl> input_tensor =
        context_->GetWebNNTensorImpl(tensor_handle);
    if (!input_tensor) {
      return;
    }

    // Input MLTensor is always dispatchable, which isn’t allowed when used as
    // a graph constant.
    if (input_tensor->usage().Has(MLTensorUsageFlags::kGraphConstant)) {
      GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }

    name_to_input_tensors.emplace_back(name, std::move(input_tensor));
  }
  base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
      name_to_input_tensor_map(std::move(name_to_input_tensors));
  if (!ValidateWebNNTensors(
          name_to_input_tensor_map,
          compute_resource_info_.input_names_to_descriptors)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Resolve the token of a output MLTensor to the corresponding `WebNNTensor`
  // instance.
  std::vector<std::pair<std::string, scoped_refptr<WebNNTensorImpl>>>
      name_to_output_tensors;
  name_to_output_tensors.reserve(named_outputs.size());
  for (const auto& [name, tensor_handle] : named_outputs) {
    scoped_refptr<WebNNTensorImpl> output_tensor =
        context_->GetWebNNTensorImpl(tensor_handle);
    if (!output_tensor) {
      return;
    }

    // Output MLTensor is always dispatchable, which isn’t allowed when used as
    // a graph constant.
    if (output_tensor->usage().Has(MLTensorUsageFlags::kGraphConstant)) {
      GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }

    name_to_output_tensors.emplace_back(name, std::move(output_tensor));
  }

  base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
      name_to_output_tensor_map(std::move(name_to_output_tensors));
  if (!ValidateWebNNTensors(
          name_to_output_tensor_map,
          compute_resource_info_.output_names_to_descriptors)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Call DispatchImpl() implemented by an `mojom::WebNNGraph` backend.
  context_->scheduler_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
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
          GetMojoReceiver().GetBadMessageCallback()));
}

}  // namespace webnn
