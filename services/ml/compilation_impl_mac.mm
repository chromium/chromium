// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_impl_mac.h"

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/compilation_impl_mac_bnns.h"
#include "services/ml/compilation_impl_mac_mps.h"
#include "services/ml/execution_impl_mac_bnns.h"
#include "services/ml/execution_impl_mac_mps.h"
#include "services/ml/mps_protocols_impl.h"
#include "services/ml/mpscnn_context.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

CompilationImplMac::CompilationImplMac(ModelImplMac* model)
    : compilation_factory_(this) {
  operands_.reserve(model->operands_.size());
  for (uint32_t i = 0; i < model->operands_.size(); ++i) {
    OperandMac operand(model->operands_[i]);
    operands_.push_back(operand);
  }
  operations_.reserve(model->operations_.size());
  for (uint32_t i = 0; i < model->operations_.size(); ++i) {
    OperationMac operation(model->operations_[i]);
    operation.filter = nullptr;
    operations_.push_back(operation);
  }
  values_ = model->values_;
  inputs_ = model->inputs_;
  outputs_ = model->outputs_;
  memory_size_ = model->memory_size_;
  memory_.reset(new int8_t[memory_size_]);
  memcpy(memory_.get(), model->memory_.get(), memory_size_);
  is_bnns_ = true;
}

CompilationImplMac::~CompilationImplMac() {
  if (is_bnns_) {
    for (size_t i = 0; i < this->operations_.size(); i++) {
      const OperationMac& operation = this->operations_[i];
      if (operation.local_operation == KBNNSFilter &&
          operation.filter != nullptr) {
        if (@available(macOS 10.12, *)) {
          BNNSFilterDestroy(operation.filter);
        }
      }
    }
  }
}

void CompilationImplMac::Finish(int32_t preference, FinishCallback callback) {
  DLOG(INFO) << "CompilationImplMac::Finish";
  DLOG(INFO) << "  "
             << "preference: " << preference;
  if (@available(macOS 10.13, *)) {
    if ((is_bnns_ = (preference == mojom::PREFER_FAST_SINGLE_ANSWER))) {
      CompileModelWithBNNS(std::move(callback));
    } else {
      CompileModelWithMPS(std::move(callback));
    }
  } else {
    std::move(callback).Run(mojom::BAD_STATE);
  }
}

void CompilationImplMac::CreateExecution(CreateExecutionCallback callback) {
  DLOG(INFO) << "CompilationImplMac::CreateExecution";
  auto init_params = mojom::ExecutionInitParams::New();

  uint32_t input_memory_size = 0;
  init_params->inputs.reserve(inputs_.size());
  for (size_t i = 0; i < inputs_.size(); ++i) {
    OperandMac& operand = operands_[inputs_[i]];
    input_memory_size += operand.requiredSize();
    init_params->inputs.push_back(
        mojom::OperandInfo::New(inputs_[i], operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required input memory size: " << input_memory_size;

  uint32_t output_memory_size = 0;
  init_params->outputs.reserve(outputs_.size());
  for (size_t i = 0; i < outputs_.size(); ++i) {
    OperandMac& operand = operands_[outputs_[i]];
    output_memory_size += operand.requiredSize();
    init_params->outputs.push_back(
        mojom::OperandInfo::New(outputs_[i], operand.type, operand.dimensions));
  }
  DLOG(INFO) << "Required output memory size: " << output_memory_size;

  mojo::ScopedSharedBufferHandle memory_handle =
      mojo::SharedBufferHandle::Create(input_memory_size + output_memory_size);

  init_params->memory =
      memory_handle->Clone(mojo::SharedBufferHandle::AccessMode::READ_WRITE);

  mojom::ExecutionPtrInfo ptr_info;
  if (is_bnns_) {
    auto impl = std::make_unique<ExecutionImplMacBNNS>(
        compilation_factory_.GetWeakPtr(), std::move(memory_handle));
    if (!impl->IsValid()) {
      std::move(callback).Run(mojom::BAD_DATA, nullptr);
      return;
    }
    mojo::MakeStrongBinding(std::move(impl), mojo::MakeRequest(&ptr_info));
  } else {
    auto impl = std::make_unique<ExecutionImplMacMPS>(
        compilation_factory_.GetWeakPtr(), std::move(memory_handle));
    if (!impl->IsValid()) {
      std::move(callback).Run(mojom::BAD_DATA, nullptr);
      return;
    }
    mojo::MakeStrongBinding(std::move(impl), mojo::MakeRequest(&ptr_info));
  }
  init_params->execution = std::move(ptr_info);

  std::move(callback).Run(mojom::NOT_ERROR, std::move(init_params));
}

API_AVAILABLE(macosx(10.13))
void CompilationImplMac::CompileModelWithBNNS(FinishCallback callback) {
  bool success = true;
  for (size_t i = 0; i < operations_.size(); ++i) {
    OperationMac& operation = operations_[i];
    uint32_t type = operation.type;
    std::vector<uint32_t>& inputs = operation.inputs;
    std::vector<uint32_t>& outputs = operation.outputs;
    DLOG(INFO) << "    inputs(" << inputs.size()
               << "): " << VectorToString(inputs.data(), inputs.size());
    DLOG(INFO) << "    outputs(" << outputs.size()
               << "): " << VectorToString(outputs.data(), outputs.size());
    // Adjust the read count
    for (size_t j = 0; j < inputs.size(); ++j) {
      OperandMac& operand = operands_[inputs[j]];
      operand.read_count += 1;
    }

    if (type == mojom::CONV_2D) {
      success = CompileConv2DBNNS(operation, values_, memory_, operands_);
    } else if (type == mojom::DEPTHWISE_CONV_2D) {
      DLOG(ERROR) << "Operation is not supported";
      success = false;
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      success =
          CompileAverageOrMaxPool2DBNNS(operation, values_, memory_, operands_);
    } else if (type == mojom::SOFTMAX) {
      success = CompileSoftmaxBNNS(operation, values_, memory_, operands_);
    } else if (type == mojom::RESHAPE) {
      success = CompileReshapeBNNS(operation);
    } else if (type == mojom::CONCATENATION) {
      success = CompileConcatenationBNNS(operation, values_, memory_,
                                         i == 0 ? true : false);
    } else if (type == mojom::ADD || type == mojom::MUL) {
      success = CompileArithmeticBNNS(operation, values_, memory_, operands_);
    } else if (type == mojom::FULLY_CONNECTED) {
      success =
          CompileFullyConnectedBNNS(operation, values_, memory_, operands_);
    } else if (type == mojom::RESIZE_BILINEAR) {
      success = CompileResizeBilinearBNNS(operation);
    } else {
      DLOG(ERROR) << "Operation is not supported";
      success = false;
    }

    if (!success)
      break;
  }

  if (success) {
    std::move(callback).Run(mojom::NOT_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

API_AVAILABLE(macosx(10.13))
void CompilationImplMac::CompileModelWithMPS(FinishCallback callback) {
  if (!GetMPSCNNContext().IsValid()) {
    std::move(callback).Run(mojom::BAD_STATE);
    return;
  }

  // Reset intermediate variable.
  graphs_.clear();
  mps_image_nodes_.clear();

  // Create a placeholder for inputs image.
  for (auto index : inputs_) {
    mps_image_nodes_[index] = [[MPSNNImageNode alloc] initWithHandle:nullptr];
  }

  bool success = true, new_graph = false;
  std::vector<uint32_t> graph_outputs;
  for (size_t i = 0; i < operations_.size(); ++i) {
    OperationMac& operation = operations_[i];
    uint32_t type = operation.type;
    std::vector<uint32_t>& inputs = operation.inputs;
    std::vector<uint32_t>& outputs = operation.outputs;
    DLOG(INFO) << "    inputs(" << inputs.size()
               << "): " << VectorToString(inputs.data(), inputs.size());
    DLOG(INFO) << "    outputs(" << outputs.size()
               << "): " << VectorToString(outputs.data(), outputs.size());
    // Adjust the read count
    for (size_t j = 0; j < inputs.size(); ++j) {
      OperandMac& operand = operands_[inputs[j]];
      operand.read_count += 1;
    }

    if (new_graph) {
      MPSNNImageNode* export_image_node = mps_image_nodes_[inputs[0]];
      export_image_node.exportFromGraph = true;
      TemporaryImageHandle* input_handle = [[TemporaryImageHandle alloc]
          initWithLabel:[NSString stringWithFormat:@"%d", inputs[0]]];
      export_image_node.handle = input_handle;
      // Create a placeholder for input image, but mps_image_nodes_[inputs[0]]
      // doesn't need reuse in new graph that does not need to reset.
      mps_image_nodes_[inputs[0]] =
          [[MPSNNImageNode alloc] initWithHandle:input_handle];

      new_graph = false;
    }

    DCHECK(outputs.size() == 1);
    if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D ||
        type == mojom::ATROUS_CONV_2D ||
        type == mojom::ATROUS_DEPTHWISE_CONV_2D) {
      success = CompileConv2DOrDepthwiseConv2D(mps_image_nodes_, operation,
                                               values_, memory_, operands_);
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      success = CompileAverageOrMaxPool2D(mps_image_nodes_, operation, values_,
                                          memory_, operands_);
    } else if (type == mojom::SOFTMAX) {
      success = CompileSoftmax(mps_image_nodes_, operation, values_, memory_);
    } else if (type == mojom::RESHAPE) {
      success = CompileReshape(operations_, operation);
    } else if (type == mojom::CONCATENATION) {
      success = CompileConcatenation(mps_image_nodes_, operations_, operation,
                                     values_, memory_, operands_);
      DLOG(ERROR) << "CONCATENATION is not supported";
    } else if (type == mojom::ADD || type == mojom::MUL) {
      success = CompileArithmetic(mps_image_nodes_, operation, operands_,
                                  constants_, values_, memory_);
    } else if (type == mojom::FULLY_CONNECTED) {
      success = CompileFullyConnected(mps_image_nodes_, operation, operands_,
                                      values_, memory_);
    } else if (type == mojom::RESIZE_BILINEAR) {
      success = CompileBilinearScale(mps_image_nodes_, operation, operands_,
                                     values_, memory_);
    } else {
      LOG(ERROR) << "Operation is not supported";
      success = false;
    }

    if (!success)
      break;

    for (size_t i = 0; i < outputs_.size(); i++) {
      if (outputs[0] == outputs_[i]) {
        new_graph = true;
        // The order of graph is not the same as outputs_.
        graph_outputs.push_back(outputs[0]);
        // Set index of output image.
        mps_image_nodes_[outputs[0]].handle = [[TemporaryImageHandle alloc]
            initWithLabel:[NSString stringWithFormat:@"%d", outputs[0]]];
      }
    }
  }

  if (success) {
    // The output image need to return result with MPSImage.
    for (size_t i = 0; i < graph_outputs.size(); i++) {
      // OutputImageAllocator* image_allocator = [[OutputImageAllocator alloc]
      // init]; mps_image_nodes_[outputs[0]].imageAllocator = image_allocator;
      // mps_image_nodes_[outputs[0]].exportFromGraph = true;

      // Multiple outputs api initWithDevice:resultImages:resultsAreNeeded: is
      // only available after 10.14.1+.
      if (@available(macOS 10.13.4, *)) {
        // The graph itself is an MPSNNGraph object and is connected to the
        // output of the very last layer in the network
        graphs_.push_back(base::scoped_nsobject<MPSNNGraph>([[MPSNNGraph alloc]
                 initWithDevice:GetMPSCNNContext().device
                    resultImage:mps_image_nodes_[graph_outputs[i]]
            resultImageIsNeeded:true]));
      } else if (@available(macOS 10.13, *)) {
        graphs_.push_back(base::scoped_nsobject<MPSNNGraph>([[MPSNNGraph alloc]
            initWithDevice:GetMPSCNNContext().device
               resultImage:mps_image_nodes_[graph_outputs[i]]]));
      }
      // DLOG(ERROR) << base::SysNSStringToUTF8([graph_ debugDescription]);
    }
    std::move(callback).Run(mojom::NOT_ERROR);
  } else {
    std::move(callback).Run(mojom::BAD_DATA);
  }
}

}  // namespace ml
