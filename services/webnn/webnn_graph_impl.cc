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
#include "services/webnn/webnn_buffer_impl.h"
#include "services/webnn/webnn_context_impl.h"
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

// Return false if the named buffers for dispatch don't match the built
// graph's expectation.
bool ValidateWebNNBuffers(
    const base::flat_map<std::string_view, WebNNBufferImpl*>& named_buffers,
    const base::flat_map<std::string, OperandDescriptor>&
        names_to_descriptors) {
  return base::ranges::equal(
      named_buffers, names_to_descriptors,
      [](const auto& named_buffer, const auto& buffer_spec) {
        const auto& [buffer_name, buffer_impl] = named_buffer;
        const auto& [buffer_spec_name, buffer_spec_descriptor] = buffer_spec;
        return buffer_name == buffer_spec_name &&
               buffer_impl->data_type() == buffer_spec_descriptor.data_type() &&
               buffer_impl->shape() == buffer_spec_descriptor.shape();
      });
}

// Return false if the same buffer was specified in inputs and outputs.
bool ValidateWebNNBuffersUsage(
    const base::flat_map<std::string, blink::WebNNBufferToken>& named_inputs,
    const base::flat_map<std::string, blink::WebNNBufferToken>& named_outputs) {
  // Validate that output buffers are unique.
  std::set<blink::WebNNBufferToken> output_buffers;
  for (const auto& named_output : named_outputs) {
    output_buffers.insert(named_output.second);
  }

  if (output_buffers.size() != named_outputs.size()) {
    return false;
  }

  // Validate buffers used for input and output are unique.
  for (const auto& named_input : named_inputs) {
    if (output_buffers.contains(named_input.second)) {
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
    const ComputeResourceInfo&) = default;
WebNNGraphImpl::ComputeResourceInfo&
WebNNGraphImpl::ComputeResourceInfo::operator=(const ComputeResourceInfo&) =
    default;

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
    const base::flat_map<std::string, blink::WebNNBufferToken>& named_inputs,
    const base::flat_map<std::string, blink::WebNNBufferToken>& named_outputs) {
  if (!ValidateWebNNBuffersUsage(named_inputs, named_outputs)) {
    mojo::ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Resolve the token of a input MLBuffer to the corresponding `WebNNBuffer`
  // instance.
  std::vector<std::pair<std::string_view, WebNNBufferImpl*>>
      name_to_input_buffers;
  name_to_input_buffers.reserve(named_inputs.size());
  for (const auto& [name, buffer_handle] : named_inputs) {
    base::optional_ref<WebNNBufferImpl> input_buffer =
        context_->GetWebNNBufferImpl(buffer_handle);
    if (!input_buffer.has_value()) {
      return;
    }
    name_to_input_buffers.emplace_back(name, input_buffer.as_ptr());
  }
  base::flat_map<std::string_view, WebNNBufferImpl*> name_to_input_buffer_map(
      std::move(name_to_input_buffers));
  if (!ValidateWebNNBuffers(
          name_to_input_buffer_map,
          compute_resource_info_.input_names_to_descriptors)) {
    mojo::ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Resolve the token of a output MLBuffer to the corresponding `WebNNBuffer`
  // instance.
  std::vector<std::pair<std::string_view, WebNNBufferImpl*>>
      name_to_output_buffers;
  name_to_output_buffers.reserve(named_outputs.size());
  for (const auto& [name, buffer_handle] : named_outputs) {
    base::optional_ref<WebNNBufferImpl> output_buffer =
        context_->GetWebNNBufferImpl(buffer_handle);
    if (!output_buffer.has_value()) {
      return;
    }
    name_to_output_buffers.emplace_back(name, output_buffer.as_ptr());
  }

  base::flat_map<std::string_view, WebNNBufferImpl*> name_to_output_buffer_map(
      std::move(name_to_output_buffers));
  if (!ValidateWebNNBuffers(
          name_to_output_buffer_map,
          compute_resource_info_.output_names_to_descriptors)) {
    mojo::ReportBadMessage(kBadMessageInvalidBuffer);
    return;
  }

  // Call DispatchImpl() implemented by an `mojom::WebNNGraph` backend.
  DispatchImpl(name_to_input_buffer_map, name_to_output_buffer_map);
}

}  // namespace webnn
