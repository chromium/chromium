// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include <math.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/dcheck_is_on.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/dml/graph_impl_dml.h"
#endif

namespace webnn {

namespace {

// Return false if the named inputs for computation don't match the built
// graph's expectation.
bool ValidateInputsForComputation(
    const base::flat_map<std::string, mojo_base::BigBuffer>& named_inputs,
    const base::flat_map<std::string, OperandDescriptor>&
        names_to_descriptors) {
  return base::ranges::equal(
      named_inputs, names_to_descriptors,
      [](const auto& input, const auto& input_spec) {
        const auto& [input_name, input_buffer] = input;
        const auto& [input_spec_name, input_spec_descriptor] = input_spec;
        return input_name == input_spec_name &&
               input_buffer.size() == input_spec_descriptor.PackedByteLength();
      });
}

// Return false if the named tensors for dispatch don't match the built
// graph's expectation.
bool ValidateWebNNTensors(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_tensors,
    const base::flat_map<std::string, OperandDescriptor>&
        names_to_descriptors) {
  return base::ranges::equal(
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
    base::PassKey<WebNNGraphBuilderImpl> pass_key)
    : input_names_to_descriptors(std::move(input_names_to_descriptors)),
      output_names_to_descriptors(std::move(output_names_to_descriptors)) {}

WebNNGraphImpl::ComputeResourceInfo::ComputeResourceInfo(
    ComputeResourceInfo&&) = default;
WebNNGraphImpl::ComputeResourceInfo&
WebNNGraphImpl::ComputeResourceInfo::operator=(ComputeResourceInfo&&) = default;

WebNNGraphImpl::ComputeResourceInfo::~ComputeResourceInfo() = default;

WebNNGraphImpl::WebNNGraphImpl(WebNNContextImpl* context,
                               ComputeResourceInfo compute_resource_info)
    : compute_resource_info_(std::move(compute_resource_info)),
      context_(context) {
  CHECK(context_);
#if DCHECK_IS_ON()
  context_->AssertCalledOnValidSequence();
#endif
}

WebNNGraphImpl::~WebNNGraphImpl() = default;

void WebNNGraphImpl::Compute(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  if (!ValidateInputsForComputation(
          named_inputs, compute_resource_info_.input_names_to_descriptors)) {
    mojo::ReportBadMessage(
        "The inputs for computation don't match the built graph's "
        "expectation.");

    // `mojo::ReportBadMessage()` will kill the renderer process, but Mojo
    // complains if the callback is not run. Just run it with nonsense
    // arguments.
    std::move(callback).Run(mojom::ComputeResult::NewError(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Unexpected inputs received from the caller.")));
    return;
  }

  // Call ComputeImpl() implemented by an `mojom::WebNNGraph` backend.
  ComputeImpl(std::move(named_inputs), std::move(callback));
}

void WebNNGraphImpl::Dispatch(
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_inputs,
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_outputs) {
  if (!ValidateWebNNTensorsUsage(named_inputs, named_outputs)) {
    mojo::ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Resolve the token of a input MLTensor to the corresponding `WebNNTensor`
  // instance.
  std::vector<std::pair<std::string_view, WebNNTensorImpl*>>
      name_to_input_tensors;
  name_to_input_tensors.reserve(named_inputs.size());
  for (const auto& [name, tensor_handle] : named_inputs) {
    base::optional_ref<WebNNTensorImpl> input_tensor =
        context_->GetWebNNTensorImpl(tensor_handle);
    if (!input_tensor.has_value()) {
      return;
    }
    name_to_input_tensors.emplace_back(name, input_tensor.as_ptr());
  }
  base::flat_map<std::string_view, WebNNTensorImpl*> name_to_input_tensor_map(
      std::move(name_to_input_tensors));
  if (!ValidateWebNNTensors(
          name_to_input_tensor_map,
          compute_resource_info_.input_names_to_descriptors)) {
    mojo::ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Resolve the token of a output MLTensor to the corresponding `WebNNTensor`
  // instance.
  std::vector<std::pair<std::string_view, WebNNTensorImpl*>>
      name_to_output_tensors;
  name_to_output_tensors.reserve(named_outputs.size());
  for (const auto& [name, tensor_handle] : named_outputs) {
    base::optional_ref<WebNNTensorImpl> output_tensor =
        context_->GetWebNNTensorImpl(tensor_handle);
    if (!output_tensor.has_value()) {
      return;
    }
    name_to_output_tensors.emplace_back(name, output_tensor.as_ptr());
  }

  base::flat_map<std::string_view, WebNNTensorImpl*> name_to_output_tensor_map(
      std::move(name_to_output_tensors));
  if (!ValidateWebNNTensors(
          name_to_output_tensor_map,
          compute_resource_info_.output_names_to_descriptors)) {
    mojo::ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Call DispatchImpl() implemented by an `mojom::WebNNGraph` backend.
  DispatchImpl(name_to_input_tensor_map, name_to_output_tensor_map);
}

}  // namespace webnn
