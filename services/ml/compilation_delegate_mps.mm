// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/compilation_delegate_mps.h"

#include "base/logging.h"
#include "base/mac/availability.h"
#include "base/mac/sdk_forward_declarations.h"
#include "services/ml/common.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/mps_protocols_impl.h"
#include "services/ml/mpscnn_context.h"
#include "services/ml/public/mojom/constants.mojom.h"

namespace ml {

API_AVAILABLE(macosx(10.13))
std::vector<uint32_t> DemensionsInNHWC(const OperandMac& operand) {
  if (operand.dimensions.size() == 0 || operand.dimensions.size() > 4) {
    DLOG(ERROR) << "dimension " << operand.dimensions.size()
                << " is not supported";
    return std::vector<uint32_t>();
  }

  std::vector<uint32_t> dim = operand.dimensions;
  int insert_count = 4 - dim.size();
  if (insert_count > 0) {
    dim.insert(dim.begin(), insert_count, 1);
  }

  return dim;
}

API_AVAILABLE(macosx(10.13))
MPSImage* CreateMPSImage(const CompiledModelMPS* compiled_model,
                         size_t index,
                         size_t extend_channels = 0) {
  const OperandMac& operand = compiled_model->operands_[index];
  if (operand.type != mojom::TENSOR_FLOAT32) {
    DLOG(ERROR) << "The data type is not supported";
    return nullptr;
  }
  std::vector<uint32_t> dimensions = DemensionsInNHWC(operand);
  if (dimensions.size() == 0) {
    DLOG(ERROR) << "Current dimension is not supported";
    return nullptr;
  }
  if (extend_channels != 0) {
    dimensions[3] = extend_channels;
  }

  MPSImageDescriptor* image_desc = [MPSImageDescriptor
      imageDescriptorWithChannelFormat:MPSImageFeatureChannelFormatFloat16
                                 width:dimensions[2]
                                height:dimensions[1]
                       featureChannels:dimensions[3]
                        numberOfImages:dimensions[0]
                                 usage:MTLTextureUsageShaderRead |
                                       MTLTextureUsageShaderWrite];
  MPSImage* mps_image =
      [[MPSImage alloc] initWithDevice:GetMPSCNNContext().device
                       imageDescriptor:image_desc];

  return mps_image;
}

API_AVAILABLE(macosx(10.13))
MPSImage* CreateMPSImageWithData(CompiledModelMPS* compiled_model,
                                 size_t index,
                                 size_t extend_channels = 0) {
  std::string index_str(base::NumberToString(index));
  ValueInfo value_info = compiled_model->values_[index_str];
  void* cpu_buffer =
      static_cast<void*>(compiled_model->memory_.get() + value_info.offset);
  MPSImage* mps_image = CreateMPSImage(compiled_model, index, extend_channels);
  if (!mps_image) {
    LOG(ERROR) << "Failed creating MPSImage for constants data.";
    return nullptr;
  }
  id<MTLCommandBuffer> command_buffer =
      [GetMPSCNNContext().command_queue commandBuffer];
  size_t length = value_info.length;
  std::vector<float> extend_data;
  if (extend_channels != 0) {
    length = extend_channels * value_info.length;
    extend_data.reserve(extend_channels);
    float* data = static_cast<float*>(cpu_buffer);
    extend_data.insert(extend_data.begin(), extend_channels, data[0]);
    cpu_buffer = extend_data.data();
  }
  UploadToMPSImage(mps_image, command_buffer, cpu_buffer, length);
  [command_buffer commit];
  [command_buffer waitUntilCompleted];

  return mps_image;
}

API_AVAILABLE(macosx(10.13))
MPSCNNNeuron* CreateMPSCNNNeuron(int32_t fuse_code) {
  MPSCNNNeuron* relu = nullptr;
  if (fuse_code == mojom::FUSED_NONE) {
    relu = nullptr;
  } else if (fuse_code == mojom::FUSED_RELU) {
    relu = [[MPSCNNNeuronReLU alloc] initWithDevice:GetMPSCNNContext().device
                                                  a:0];
  } else if (fuse_code == mojom::FUSED_RELU1) {
    relu = [[MPSCNNNeuronReLUN alloc] initWithDevice:GetMPSCNNContext().device
                                                   a:0
                                                   b:1];
  } else if (fuse_code == mojom::FUSED_RELU6) {
    relu = [[MPSCNNNeuronReLUN alloc] initWithDevice:GetMPSCNNContext().device
                                                   a:0
                                                   b:6];
  } else {
    DLOG(INFO) << "Fuse code " << fuse_code
               << " is not supported by MPSCNNNeuron";
  }
  return relu;
}

API_AVAILABLE(macosx(10.13))
MPSCNNConvolutionNode* CreateMPSCNNConvolutionNode(MPSNNImageNode* image_node,
                                                   int32_t filter_width,
                                                   int32_t filter_height,
                                                   int32_t depth_in,
                                                   int32_t depth_out,
                                                   int32_t stride_width,
                                                   int32_t stride_height,
                                                   const float* weights,
                                                   const float* bias,
                                                   MPSCNNNeuron* relu,
                                                   int32_t type,
                                                   int32_t dilation_x = 1,
                                                   int32_t dilation_y = 1) {
  Class descriptor_class =
      (type == mojom::DEPTHWISE_CONV_2D ||
       type == mojom::ATROUS_DEPTHWISE_CONV_2D)
          ? NSClassFromString(@"MPSCNNDepthWiseConvolutionDescriptor")
          : NSClassFromString(@"MPSCNNConvolutionDescriptor");
  const MPSCNNConvolutionDescriptor* desc =
      [descriptor_class cnnConvolutionDescriptorWithKernelWidth:filter_width
                                                   kernelHeight:filter_height
                                           inputFeatureChannels:depth_in
                                          outputFeatureChannels:depth_out
                                                   neuronFilter:relu];
  desc.strideInPixelsX = stride_width;
  desc.strideInPixelsY = stride_height;
  desc.dilationRateX = dilation_x;
  desc.dilationRateY = dilation_y;
  desc.groups = 1;

  auto data_source = [[ConvDataSource alloc]
      initWithWeight:(float*)weights
                bias:(float*)bias
                desc:(MPSCNNConvolutionDescriptor*)desc];
  Class convolution_class = type == mojom::FULLY_CONNECTED
                                ? NSClassFromString(@"MPSCNNFullyConnectedNode")
                                : NSClassFromString(@"MPSCNNConvolutionNode");
  return [[convolution_class alloc] initWithSource:image_node
                                           weights:data_source];
}

API_AVAILABLE(macosx(10.13))
void ComputeMPSOffsetForImplictPadding(bool same_padding,
                                       MPSOffset& offset,
                                       int32_t input_height,
                                       int32_t input_width,
                                       int32_t output_height,
                                       int32_t output_width,
                                       int32_t filter_height,
                                       int32_t filter_width,
                                       int32_t stride_height,
                                       int32_t stride_width) {
  if (same_padding) {
    int pad_along_height =
        ((output_height - 1) * stride_height + filter_height - input_height);
    int pad_along_width =
        ((output_width - 1) * stride_width + filter_width - input_width);
    int pad_top = (int)(pad_along_height / 2);
    int pad_left = (int)(pad_along_width / 2);

    offset.x = (int)(filter_width / 2) - pad_left;
    offset.y = (int)(filter_height / 2) - pad_top;
    offset.z = 0;
  } else {
    offset.x = (int)(filter_width / 2);
    offset.y = (int)(filter_height / 2);
    offset.z = 0;
  }
}

CompiledModelMPS::CompiledModelMPS() = default;

CompiledModelMPS::~CompiledModelMPS() = default;

CompilationDelegateMPS::CompilationDelegateMPS(
    const CompilationImpl* compilation)
    : CompilationDelegate(), compilation_(compilation) {
  compiled_model_ = base::MakeRefCounted<CompiledModelMPS>();
}

CompilationDelegateMPS::~CompilationDelegateMPS() {}

int32_t CompilationDelegateMPS::CreateExecution(
    std::unique_ptr<mojom::Execution>& execution,
    mojom::ExecutionInitParamsPtr params) {
  execution =
      std::make_unique<ExecutionImplMPS>(compiled_model_, std::move(params));
  return mojom::NOT_ERROR;
}

int32_t CompilationDelegateMPS::Compile() {
  const mojom::ModelInfoPtr& model = compilation_->GetModel();
  CompileForModel(model, compiled_model_.get());
  uint32_t memory_size_ = model->memory_size;
  if (memory_size_ > 0) {
    auto mapping = model->memory->Map(memory_size_);
    const int8_t* base = static_cast<const int8_t*>(mapping.get());
    compiled_model_->memory_.reset(new int8_t[memory_size_]);
    memcpy(compiled_model_->memory_.get(), base, memory_size_);
  }

  // TODO:Move Create constants MPSImage from execution(l.172 - l.189)
  // in Compilation so that those section (l.135 - l. 147) can be removed.
  for (auto iter = model->values.begin(); iter != model->values.end(); iter++) {
    std::string key = iter->first;
    ValueInfo value_info;
    value_info.index = iter->second->index;
    value_info.offset = iter->second->offset;
    value_info.length = iter->second->length;
    compiled_model_->values_[key] = value_info;
  }

  // Reset intermediate variable.
  compiled_model_->graphs_.clear();
  compiled_model_->mps_image_nodes_.clear();

  // Create a placeholder for inputs image.
  for (auto index : compiled_model_->inputs_) {
    MPSImage* mps_image = CreateMPSImage(compiled_model_.get(), index);
    if (!mps_image) {
      LOG(ERROR) << "Failed creating MPSImage for input data.";
      return mojom::OP_FAILED;
    }
    MPSImageHandle* handle = [[MPSImageHandle alloc] initWithImage:mps_image
                                                             index:index];
    compiled_model_->mps_image_nodes_[index] =
        [[MPSNNImageNode alloc] initWithHandle:handle];
  }

  bool success = true, new_graph = false;
  std::vector<uint32_t> graph_outputs;
  for (size_t i = 0; i < compiled_model_->operations_.size(); ++i) {
    const mojom::OperationPtr& operation = model->operations[i];
    // OperationMac& operation = compiled_model_->operations_[i];
    uint32_t type = operation->type;
    std::vector<uint32_t>& inputs = operation->inputs;
    std::vector<uint32_t>& outputs = operation->outputs;
    // Adjust the read count
    for (size_t j = 0; j < inputs.size(); ++j) {
      OperandMac& operand = compiled_model_->operands_[inputs[j]];
      operand.read_count += 1;
    }

    if (new_graph) {
      MPSNNImageNode* export_image_node =
          compiled_model_->mps_image_nodes_[inputs[0]];
      export_image_node.exportFromGraph = true;
      MPSImageHandle* input_handle = [[MPSImageHandle alloc]
          initWithLabel:[NSString stringWithFormat:@"%d", inputs[0]]];
      export_image_node.handle = input_handle;
      // Create a placeholder for input image, but mps_image_nodes_[inputs[0]]
      // doesn't need reuse in new graph that does not need to reset.
      compiled_model_->mps_image_nodes_[inputs[0]] =
          [[MPSNNImageNode alloc] initWithHandle:input_handle];

      new_graph = false;
    }

    DCHECK(outputs.size() == 1);
    if (type == mojom::CONV_2D || type == mojom::DEPTHWISE_CONV_2D ||
        type == mojom::ATROUS_CONV_2D ||
        type == mojom::ATROUS_DEPTHWISE_CONV_2D) {
      success = CompileConv2DOrDepthwiseConv2D(
          compiled_model_->mps_image_nodes_, model, operation);
    } else if (type == mojom::AVERAGE_POOL_2D || type == mojom::MAX_POOL_2D) {
      success = CompileAverageOrMaxPool2D(compiled_model_->mps_image_nodes_,
                                          model, operation);
    } else if (type == mojom::SOFTMAX) {
      success =
          CompileSoftmax(compiled_model_->mps_image_nodes_, model, operation);
    } else if (type == mojom::RESHAPE) {
      success =
          CompileReshape(compiled_model_->mps_image_nodes_, model, operation);
    } else if (type == mojom::CONCATENATION) {
      success = CompileConcatenation(compiled_model_->mps_image_nodes_, model,
                                     operation);
      // LOG(ERROR) << "CONCATENATION is not supported";
    } else if (type == mojom::ADD || type == mojom::MUL) {
      success = CompileArithmetic(compiled_model_->mps_image_nodes_, model,
                                  operation);
    } else if (type == mojom::FULLY_CONNECTED) {
      success = CompileFullyConnected(compiled_model_->mps_image_nodes_, model,
                                      operation);
    } else if (type == mojom::RESIZE_BILINEAR) {
      success = CompileBilinearScale(compiled_model_->mps_image_nodes_, model,
                                     operation);
    } else if (type == mojom::ARGMAX) {
      success = CompileArgmax(compiled_model_->mps_image_nodes_, operation);
    } else if (type == mojom::LOGISTIC) {
      success = CompileSigmoid(compiled_model_->mps_image_nodes_, operation);
    } else if (type == mojom::PRELU) {
      success =
          CompilePReLU(compiled_model_->mps_image_nodes_, model, operation);
    } else {
      LOG(ERROR) << "Operation is not supported";
      success = false;
    }

    if (!success)
      break;

    for (size_t i = 0; i < compiled_model_->outputs_.size(); i++) {
      if (outputs[0] == compiled_model_->outputs_[i]) {
        new_graph = true;
        // The order of graph is not the same as outputs_.
        graph_outputs.push_back(outputs[0]);
        // Set index of output image.
        compiled_model_->mps_image_nodes_[outputs[0]].handle =
            [[MPSImageHandle alloc]
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
      if (@available(macOS 10.13.4, *)) {
        // The graph itself is an MPSNNGraph object and is connected to the
        // output of the very last layer in the network
        compiled_model_->graphs_.push_back(
            base::scoped_nsobject<MPSNNGraph>([[MPSNNGraph alloc]
                     initWithDevice:GetMPSCNNContext().device
                        resultImage:compiled_model_
                                        ->mps_image_nodes_[graph_outputs[i]]
                resultImageIsNeeded:true]));
      } else if (@available(macOS 10.13, *)) {
        compiled_model_->graphs_.push_back(
            base::scoped_nsobject<MPSNNGraph>([[MPSNNGraph alloc]
                initWithDevice:GetMPSCNNContext().device
                   resultImage:compiled_model_
                                   ->mps_image_nodes_[graph_outputs[i]]]));
      }
    }

    return mojom::NOT_ERROR;
  } else {
    return mojom::OP_FAILED;
  }
}

bool CompilationDelegateMPS::CompileConv2DOrDepthwiseConv2D(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMac::CompileConv2DOrDepthwiseConv2D "
             << operation->type;
  ConvParams params;
  int32_t result = compilation_->GetConvParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  // bool implicit_padding;
  std::vector<uint32_t> inputs = operation->inputs;

  // TODO(junwei.fu):Use ConvParams to refactor code.
  params.dilation_width = 1;
  params.dilation_height = 1;
  if (operation->type == mojom::ATROUS_DEPTHWISE_CONV_2D ||
      operation->type == mojom::ATROUS_CONV_2D) {
    LOG(ERROR) << "operation->type == mojom::ATROUS_DEPTHWISE_CONV_2D "
                  "||operation->type == mojom::ATROUS_CONV_2D";
    params.dilation_width = params.stride_width;
    params.dilation_height = params.stride_height;
    params.stride_width = 1;
    params.stride_height = 1;
  }

  const mojom::OperandValueInfoPtr& weights_value_info =
      model->values[base::NumberToString(inputs[1])];
  float* weights =
      (float*)(compiled_model_->memory_.get() + weights_value_info->offset);
  MPSNNImageNode* input_image = image_nodes[inputs[0]];
  if (params.depthwise) {
    if (params.depthwise_multiplier != 1) {
      return false;
    }
    if (params.output_channel !=
        params.input_channel * params.depthwise_multiplier) {
      DLOG(INFO)
          << "Failed assertion: outputFeatureChannels " << params.output_channel
          << " in MPS depthwise convolution descriptor must be multiplie of "
             "inFeatureChannels "
          << params.input_channel;
      return false;
    }
    // Convert from WebML weights shape [1, filter_height, filter_width,
    // depth_out] to MPSCNNConvlution weight[ outputChannels ][ kernelHeight
    // ][ kernelWidth ][ inputChannels / groups ]
    const uint32_t depthwise_weights_length =
        1 * params.filter_height * params.filter_width * params.output_channel;
    std::vector<float> depthwise_weights(depthwise_weights_length);
    for (uint32_t h = 0; h < params.filter_height; ++h) {
      for (uint32_t w = 0; w < params.filter_width; ++w) {
        for (uint32_t c = 0; c < params.output_channel; ++c) {
          depthwise_weights[c * params.filter_height * params.filter_width +
                            h * params.filter_width + w] =
              weights[h * params.filter_width * params.output_channel +
                      w * params.output_channel + c];
        }
      }
    }
    memcpy(weights, depthwise_weights.data(), weights_value_info->length);
  }
  const mojom::OperandValueInfoPtr& bias_value_info =
      model->values[base::NumberToString(inputs[2])];
  const float* bias = reinterpret_cast<const float*>(
      compiled_model_->memory_.get() + bias_value_info->offset);
  MPSCNNConvolutionNode* conv_node = CreateMPSCNNConvolutionNode(
      input_image, params.filter_width, params.filter_height,
      params.input_channel, params.output_channel, params.stride_width,
      params.stride_height, weights, bias, CreateMPSCNNNeuron(params.fuse_code),
      operation->type, params.dilation_width, params.dilation_height);

  MPSOffset offset;
  int32_t padding_code;
  if ((!params.depthwise && inputs.size() == 7) ||
      (params.depthwise && inputs.size() == 8)) {
    padding_code = compilation_->GetScalarInt32(inputs[3]);
    ComputeMPSOffsetForImplictPadding(
        padding_code == mojom::PADDING_SAME, offset, params.input_height,
        params.input_width, params.output_height, params.output_width,
        params.filter_height, params.filter_width, params.stride_height,
        params.stride_width);
  } else if ((!params.depthwise && inputs.size() == 10) ||
             (params.depthwise && inputs.size() == 11)) {
    offset.x = (int)(params.filter_width / 2) - params.padding_left;
    offset.y = (int)(params.filter_height / 2) - params.padding_top;
    offset.z = 0;
  }

  DLOG(INFO) << "    offset MPSOffset(x: " << offset.x << " y: " << offset.y
             << ")";
  DLOG(INFO) << "    offset MPSOffset(x: " << offset.x << " y: " << offset.y
             << ")";

  // operands[outputs[0]] is output operand.
  const std::vector<uint32_t>& outputs = operation->outputs;
  std::vector<uint32_t> dimensions =
      DemensionsInNHWC(compiled_model_->operands_[outputs[0]]);
  if (dimensions.size() == 0) {
    DLOG(ERROR) << "Current dimension is not supported";
    return false;
  }
  [conv_node setPaddingPolicy:[[CustomPadding alloc]
                                  initWithOffset:offset
                                        edgeMode:MPSImageEdgeModeZero
                                             num:dimensions[0]
                                           width:dimensions[2]
                                          height:dimensions[1]
                                        channels:dimensions[3]]];
  image_nodes[outputs[0]] = conv_node.resultImage;

  return true;
}

bool CompilationDelegateMPS::CompileAverageOrMaxPool2D(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMPS::CompileAverageOrMaxPool2D";
  DLOG_IF(FATAL, operation->type != mojom::AVERAGE_POOL_2D &&
                     operation->type != mojom::MAX_POOL_2D);

  PoolingParams params;
  int32_t result = compilation_->GetPoolingParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }
  if (params.fuse_code != mojom::FUSED_NONE) {
    DLOG(ERROR) << "  fuse_code " << params.fuse_code << " is not supproted.";
    return false;
  }

  MPSCNNPoolingNode* pool_node;
  std::vector<uint32_t> inputs = operation->inputs;
  MPSNNImageNode* input_image = image_nodes[inputs[0]];
  if (operation->type == mojom::AVERAGE_POOL_2D) {
    pool_node =
        [[MPSCNNPoolingAverageNode alloc] initWithSource:input_image
                                             kernelWidth:params.filter_width
                                            kernelHeight:params.filter_height
                                         strideInPixelsX:params.stride_width
                                         strideInPixelsY:params.stride_height];
  } else if (operation->type == mojom::MAX_POOL_2D) {
    pool_node =
        [[MPSCNNPoolingMaxNode alloc] initWithSource:input_image
                                         kernelWidth:params.filter_width
                                        kernelHeight:params.filter_height
                                     strideInPixelsX:params.stride_width
                                     strideInPixelsY:params.stride_height];
  } else {
    DLOG(ERROR) << "Operation " << operation->type << " is not supported";
    return false;
  }
  MPSOffset offset;
  int32_t padding_code;
  if (inputs.size() == 7) {
    padding_code = compilation_->GetScalarInt32(inputs[1]);
    ComputeMPSOffsetForImplictPadding(
        padding_code == mojom::PADDING_SAME, offset, params.input_height,
        params.input_width, params.output_height, params.output_width,
        params.filter_height, params.filter_width, params.stride_height,
        params.stride_width);
  } else if (inputs.size() == 10) {
    offset.x = (int)(params.filter_width / 2) - params.padding_left;
    offset.y = (int)(params.filter_height / 2) - params.padding_top;
    offset.z = 0;
  } else {
    DLOG(ERROR) << "  inputs size is incorrect";
    return false;
  }

  DLOG(INFO) << "  MPSOffset x: " << offset.x << " y: " << offset.y;
  const std::vector<uint32_t>& outputs = operation->outputs;
  std::vector<uint32_t> dimensions =
      DemensionsInNHWC(compiled_model_->operands_[outputs[0]]);
  if (dimensions.size() == 0) {
    DLOG(ERROR) << "Current dimension is not supported";
    return false;
  }
  [pool_node setPaddingPolicy:[[CustomPadding alloc]
                                  initWithOffset:offset
                                        edgeMode:MPSImageEdgeModeClamp
                                             num:dimensions[0]
                                           width:dimensions[2]
                                          height:dimensions[1]
                                        channels:dimensions[3]]];
  image_nodes[outputs[0]] = pool_node.resultImage;

  return true;
}

bool CompilationDelegateMPS::CompileSoftmax(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMPS::CompileSoftmax";
  DLOG_IF(FATAL, operation->type != mojom::SOFTMAX);

  SoftmaxParams params;
  int32_t result = compilation_->GetSoftmaxParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  if (params.beta != 1.0) {
    DLOG(ERROR) << "  beta " << params.beta << " is not supported.";
    return false;
  }

  MPSCNNSoftMaxNode* softmax_node = [[MPSCNNSoftMaxNode alloc]
      initWithSource:image_nodes[operation->inputs[0]]];
  image_nodes[operation->outputs[0]] = softmax_node.resultImage;

  return true;
}

bool CompilationDelegateMPS::CompileReshape(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMPS::CompileReshape";

  uint32_t output_index = operation->outputs[0];
  if (@available(macOS 10.14.1, *)) {
    const OperandMac& output_operand = compiled_model_->operands_[output_index];
    std::vector<uint32_t> dimensions = DemensionsInNHWC(output_operand);
    if (dimensions.size() == 0) {
      DLOG(ERROR) << "Current dimension is not supported";
      return false;
    }
    MPSNNReshapeNode* reshape_node = [[MPSNNReshapeNode alloc]
               initWithSource:image_nodes[operation->inputs[0]]
                  resultWidth:dimensions[2]
                 resultHeight:dimensions[1]
        resultFeatureChannels:dimensions[3]];
    image_nodes[output_index] = reshape_node.resultImage;
  } else {
    uint32_t input_index = operation->inputs[0];
    for (size_t i = 0; i < compiled_model_->operations_.size(); ++i) {
      OperationMac& operation = compiled_model_->operations_[i];
      if (operation.inputs[0] == output_index) {
        operation.inputs[0] = input_index;
      }
    }
  }

  return true;
}

bool CompilationDelegateMPS::CompileConcatenation(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& concat) {
  DLOG(INFO) << "CompilationImplMPS::CompileConcatenation";
  DLOG_IF(FATAL, concat->type != mojom::CONCATENATION);

  ConcatParams params;
  int32_t result = compilation_->GetConcatParams(concat, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  std::vector<uint32_t> inputs = concat->inputs;
  std::vector<uint32_t> outputs = concat->outputs;

  if (params.axis != 3) {
    LOG(ERROR) << "Only axis == 3 is supported";
    return false;
  }

  NSMutableArray<MPSNNImageNode*>* image_array =
      [NSMutableArray arrayWithCapacity:1];
  for (size_t i = 0; i < inputs.size() - 1; ++i) {
    uint32_t concat_input_idx = inputs[i];
    const OperandMac& operand = compiled_model_->operands_[concat_input_idx];
    if (operand.dimensions.size() < 4) {
      LOG(ERROR) << "Invalid dimensions of operand " << concat_input_idx
                 << " length is " << operand.dimensions.size();
      return false;
    }

    MPSNNImageNode* input_node;
    std::string input_id(base::NumberToString(concat_input_idx));
    if (model->values.find(input_id) != model->values.end()) {
      compiled_model_->constants_.push_back(concat_input_idx);
      // Create a placeholder for input constant image.
      MPSImage* mps_image =
          CreateMPSImageWithData(compiled_model_.get(), concat_input_idx);
      if (!mps_image) {
        LOG(ERROR) << "Failed creating MPSImage for input data.";
        return false;
      }
      input_node = [[MPSNNImageNode alloc]
          initWithHandle:[[MPSImageHandle alloc]
                             initWithImage:mps_image
                                     index:concat_input_idx]];
    } else {
      input_node = image_nodes[concat_input_idx];
    }
    [image_array addObject:input_node];
  }

  MPSNNConcatenationNode* concat_node =
      [[MPSNNConcatenationNode alloc] initWithSources:image_array];
  // concat.outputs[0] is index of output.
  image_nodes[concat->outputs[0]] = concat_node.resultImage;

  return true;
}

bool CompilationDelegateMPS::CompileArithmetic(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationImplMPS::CompileArithmetic";

  ElementWiseParams params;
  int32_t result = compilation_->GetElementWiseParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  size_t primary_index = operation->inputs[0];
  std::vector<uint32_t> primary_dimension =
      DemensionsInNHWC(compiled_model_->operands_[primary_index]);
  size_t secondary_index = operation->inputs[1];
  std::vector<uint32_t> secondary_dimension =
      DemensionsInNHWC(compiled_model_->operands_[secondary_index]);
  if (primary_dimension[0] != secondary_dimension[0]) {
    LOG(ERROR) << "Different batch size for arithmetic isn't supported.";
    return false;
  }

  size_t extend_channles = 0;
  // Check constants for input 0 and 1
  NSMutableArray<MPSNNImageNode*>* image_array =
      [NSMutableArray arrayWithCapacity:1];
  for (size_t i = 0; i < 2; ++i) {
    size_t input_index = operation->inputs[i];
    std::string input_id(base::NumberToString(operation->inputs[i]));
    if (model->values.find(input_id) != model->values.end()) {
      compiled_model_->constants_.push_back(input_index);
      // Extend feature channel data for broadcasting.
      if (i == 0) {
        if (primary_dimension[3] == 1 && product(primary_dimension) == 1 &&
            secondary_dimension[3] > 1) {
          extend_channles = secondary_dimension[3];
        }
      } else if (i == 1) {
        if (secondary_dimension[3] == 1 && product(secondary_dimension) == 1 &&
            primary_dimension[3] > 1) {
          extend_channles = primary_dimension[3];
        }
      }
      // Create a placeholder for input constant image.
      MPSImage* mps_image = CreateMPSImageWithData(
          compiled_model_.get(), input_index, extend_channles);
      if (!mps_image) {
        LOG(ERROR) << "Failed creating MPSImage for constant data.";
        return false;
      }
      MPSImageHandle* handle =
          [[MPSImageHandle alloc] initWithImage:mps_image index:input_index];
      image_nodes[input_index] = [[MPSNNImageNode alloc] initWithHandle:handle];
    }
  }

  if (primary_dimension[3] == secondary_dimension[3] || extend_channles != 0) {
    if (product(primary_dimension) > product(secondary_dimension)) {
      // Support feature channle axis broadcasting.
      // For example: input0{1, 1, 3, 2} input1{1, 1, 1, 2}, we should add
      // input1 before input0 to use broadcasting.
      [image_array addObject:image_nodes[secondary_index]];
      [image_array addObject:image_nodes[primary_index]];
    } else {
      [image_array addObject:image_nodes[primary_index]];
      [image_array addObject:image_nodes[secondary_index]];
    }
  } else {
    LOG(ERROR) << "The broadcasting isn't supported currently.";
    return false;
  }

  MPSNNBinaryArithmeticNode* arithmetic_node = nullptr;
  if (operation->type == mojom::ADD) {
    arithmetic_node = [[MPSNNAdditionNode alloc] initWithSources:image_array];
  } else if (operation->type == mojom::MUL) {
    arithmetic_node =
        [[MPSNNMultiplicationNode alloc] initWithSources:image_array];
  }

  // TODO(junwei): the activation function must be configured in index 2.
  MPSCNNNeuronNode* relu_node = nullptr;
  switch (params.fuse_code) {
    case mojom::FUSED_NONE:
      break;
    case mojom::FUSED_RELU:
      [arithmetic_node setMinimumValue:0];
      relu_node = [[MPSCNNNeuronReLUNode alloc]
          initWithSource:arithmetic_node.resultImage
                       a:0];
      break;
    case mojom::FUSED_RELU1:
      [arithmetic_node setMinimumValue:-1];
      [arithmetic_node setMaximumValue:1];
      relu_node = [[MPSCNNNeuronReLUNNode alloc]
          initWithSource:arithmetic_node.resultImage
                       a:0
                       b:1];
      break;
    case mojom::FUSED_RELU6:
      [arithmetic_node setMinimumValue:0];
      [arithmetic_node setMaximumValue:6];
      relu_node = [[MPSCNNNeuronReLUNNode alloc]
          initWithSource:arithmetic_node.resultImage
                       a:0
                       b:6];
      break;
    default:
      NOTREACHED();
  }

  image_nodes[operation->outputs[0]] =
      relu_node ? relu_node.resultImage : arithmetic_node.resultImage;

  return true;
}

bool CompilationDelegateMPS::CompileFullyConnected(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "CompilationDelegateMPS::CompileFullyConnected";

  FullyConnectedParams params;
  int32_t result = compilation_->GetFullyConnectedParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  // operation.inputs[0] is the index of input in operands_.
  OperandMac& input = compiled_model_->operands_[operation->inputs[0]];
  if (input.dimensions.size() < 2) {
    DLOG(ERROR) << "A tenosr of least rank 2.";
    return false;
  }

  // If rank is greater than 2, then it gets flattened to a 2-D Tensor.
  // input_size corresponds to the number of inputs to the layer, matching
  // the second dimension of weights.
  // It is the same as input.dimensions[1] in 2-d inputs.
  // inputs[1] is index of weights.
  // operands_[inputs[1]].dimensions[1] is the second dimension of weights.
  // batch_size is calculated by dividing the number of elements by input_size.
  const std::vector<uint32_t>& inputs = operation->inputs;
  input.dimensions =
      std::vector<uint32_t>({params.input_batch_size, params.input_size});
  // inputs[1] is index of weights, values_.at(inputs[1]) is value info
  // of weights.
  const mojom::OperandValueInfoPtr& weights_value_info =
      model->values[base::NumberToString(inputs[1])];
  const float* source_weights = reinterpret_cast<const float*>(
      compiled_model_->memory_.get() + weights_value_info->offset);
  // inputs[2] is index of bias, values_.at(inputs[2]) is value info of
  // bias.
  const mojom::OperandValueInfoPtr& bias_value_info =
      model->values[base::NumberToString(inputs[2])];
  const float* source_bias = reinterpret_cast<const float*>(
      compiled_model_->memory_.get() + bias_value_info->offset);

  MPSCNNConvolutionNode* fully_connected_node = CreateMPSCNNConvolutionNode(
      image_nodes[inputs[0]], 1, 1, params.input_size, params.output_num_units,
      1, 1, source_weights, source_bias, CreateMPSCNNNeuron(params.fuse_code),
      operation->type);

  image_nodes[operation->outputs[0]] = fully_connected_node.resultImage;

  return true;
}

bool CompilationDelegateMPS::CompileBilinearScale(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "Compile resize bilinear operation.";

  ResizeBilinearParams params;
  int32_t result = compilation_->GetResizeBilinearParams(operation, params);
  if (result != mojom::NOT_ERROR) {
    return false;
  }

  const OperandMac& output_operand =
      compiled_model_->operands_[operation->outputs[0]];
  if (output_operand.dimensions.size() != 4) {
    LOG(ERROR) << "Input and output must be 4-D tensor.";
    return false;
  }

  const OperandMac& input_operand =
      compiled_model_->operands_[operation->inputs[0]];
  if (output_operand.dimensions[2] % input_operand.dimensions[2] != 0 ||
      output_operand.dimensions[1] % input_operand.dimensions[1] != 0) {
    LOG(ERROR) << "The upsampling factor for the x/y must be integer.";
    return false;
  }

  MPSCNNUpsamplingBilinearNode* bilinear_scale_node;
  if (@available(macOS 10.14, *)) {
    bilinear_scale_node = [[MPSCNNUpsamplingBilinearNode alloc]
             initWithSource:image_nodes[operation->inputs[0]]
        integerScaleFactorX:params.x_scale
        integerScaleFactorY:params.y_scale
               alignCorners:params.align_corners];
    image_nodes[operation->outputs[0]] = bilinear_scale_node.resultImage;
    return true;
  }

  if (@available(macOS 10.13, *)) {
    LOG(WARNING) << "Only support false alignCorners on 10.13.";
    bilinear_scale_node = [[MPSCNNUpsamplingBilinearNode alloc]
             initWithSource:image_nodes[operation->inputs[0]]
        integerScaleFactorX:params.x_scale
        integerScaleFactorY:params.y_scale];
    image_nodes[operation->outputs[0]] = bilinear_scale_node.resultImage;
  }

  return true;
}

bool CompilationDelegateMPS::CompileArgmax(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "Compile argmax operation.";
  ArgmaxParams params;
  int32_t result = compilation_->GetArgmaxParams(operation, params);
  if (result != mojom::NOT_ERROR)
    return false;

  size_t input_index = operation->inputs[0];
  const OperandMac& input_operand = compiled_model_->operands_[input_index];
  if (params.axis + 1 != static_cast<int>(input_operand.dimensions.size())) {
    LOG(ERROR) << "Only support channel axis.";
    return false;
  }

  if (@available(macOS 10.14.1, *)) {
    MPSNNReductionFeatureChannelsArgumentMaxNode* argmax_node =
        [[MPSNNReductionFeatureChannelsArgumentMaxNode alloc]
            initWithSource:image_nodes[input_index]];
    image_nodes[operation->outputs[0]] = argmax_node.resultImage;
  } else {
    LOG(ERROR) << "Argmax only support above 10.14.1";
    return false;
  }

  return true;
}

bool CompilationDelegateMPS::CompileSigmoid(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "Compile sigmoid operation.";

  MPSCNNNeuronSigmoidNode* sigmoid_node = [[MPSCNNNeuronSigmoidNode alloc]
      initWithSource:image_nodes[operation->inputs[0]]];
  image_nodes[operation->outputs[0]] = sigmoid_node.resultImage;

  return true;
}

bool CompilationDelegateMPS::CompilePReLU(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const mojom::ModelInfoPtr& model,
    const mojom::OperationPtr& operation) {
  DLOG(INFO) << "Compile sigmoid PReLU.";

  NSMutableData* slope_data = [NSMutableData dataWithCapacity:0];
  size_t slope_index = operation->inputs[1];
  std::string slope_id(base::NumberToString(slope_index));
  if (model->values.find(slope_id) != model->values.end()) {
    const mojom::OperandValueInfoPtr& slope_value_info =
        model->values[slope_id];
    if (slope_value_info->length % sizeof(float) > 0) {
      LOG(ERROR) << "Invalid data that's not a multiple of sizeof(float).";
      return false;
    }
    auto mapping = compilation_->MapMemory(slope_index);
    [slope_data appendBytes:mapping.get() length:slope_value_info->length];
  }

  size_t input_index = operation->inputs[0];
  const OperandMac& input_operand = compiled_model_->operands_[input_index];
  const OperandMac& slope_operand = compiled_model_->operands_[slope_index];
  // Only have feature channel.
  if (slope_operand.dimensions.size() == 1) {
    std::vector<uint32_t> dimensions = DemensionsInNHWC(input_operand);
    if (dimensions.size() == 0) {
      DLOG(ERROR) << "Current dimension is not supported";
      return false;
    }
    uint32_t channels = dimensions[3];
    if (slope_operand.dimensions[0] == 1 && channels > 1) {
      float data;
      [slope_data getBytes:&data length:sizeof(float)];
      std::vector<float> append_channles(channels - 1, data);
      [slope_data appendBytes:append_channles.data()
                       length:sizeof(float) * append_channles.size()];
    }

    // f(x) = x                if x >= 0
    //      = aData[i] * x     if x < 0,  i is the index of the feature channel.
    MPSCNNNeuronPReLUNode* prelu_node =
        [[MPSCNNNeuronPReLUNode alloc] initWithSource:image_nodes[input_index]
                                                aData:slope_data];
    image_nodes[operation->outputs[0]] = prelu_node.resultImage;
  } else if (slope_operand.dimensions == input_operand.dimensions) {
    size_t reshape_channel = product(input_operand.dimensions);
    if (reshape_channel < 2048) {
      if (@available(macOS 10.14.1, *)) {
        // Reshape for PReLU for each feature channel.
        MPSNNReshapeNode* reshape_for_prelu =
            [[MPSNNReshapeNode alloc] initWithSource:image_nodes[input_index]
                                         resultWidth:1
                                        resultHeight:1
                               resultFeatureChannels:reshape_channel];

        MPSCNNNeuronPReLUNode* prelu_node = [[MPSCNNNeuronPReLUNode alloc]
            initWithSource:reshape_for_prelu.resultImage
                     aData:slope_data];

        // Reshape back.
        MPSNNReshapeNode* reshape_back = [[MPSNNReshapeNode alloc]
                   initWithSource:prelu_node.resultImage
                      resultWidth:input_operand.dimensions[2]
                     resultHeight:input_operand.dimensions[1]
            resultFeatureChannels:input_operand.dimensions[3]];
        image_nodes[operation->outputs[0]] = reshape_back.resultImage;
      }
    } else {
      LOG(ERROR) << "This MPSImage (" << reshape_channel
                 << "slices) would exceed the maximum allowed number of slices "
                    "in a Metal texture array (2048 slices)";
      return false;
    }
  } else {
    LOG(ERROR) << "Broadcasting isn't supported.";
    return false;
  }

  return true;
}

}  // namespace ml
