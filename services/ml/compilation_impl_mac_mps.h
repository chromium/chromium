// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_MAC_MPS_H_
#define SERVICES_ML_COMPILATION_IMPL_MAC_MPS_H_

#include <map>
#include <memory>
#include <vector>

#include "services/ml/common.h"
#include "services/ml/ml_utils_mac.h"

class CompilationImplMac;
@class MPSNNImageNode;
@class MPSImageDescriptor;

namespace ml {

API_AVAILABLE(macosx(10.13))
bool CompileConv2DOrDepthwiseConv2D(
    std::map<uint32_t, MPSNNImageNode*>& image_nodes,
    const OperationMac&,
    const std::map<uint32_t, ValueInfo>& values,
    std::unique_ptr<int8_t[]>& memory,
    const std::vector<OperandMac>& operands);

API_AVAILABLE(macosx(10.13))
bool CompileAverageOrMaxPool2D(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                               const OperationMac& operation,
                               const std::map<uint32_t, ValueInfo>& values,
                               const std::unique_ptr<int8_t[]>& memory,
                               const std::vector<OperandMac>& operands);

API_AVAILABLE(macosx(10.13))
bool CompileSoftmax(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                    const OperationMac& operation,
                    const std::map<uint32_t, ValueInfo>& values,
                    const std::unique_ptr<int8_t[]>& memory);

bool CompileReshape(std::vector<OperationMac>& operations,
                    const OperationMac& reshape);

API_AVAILABLE(macosx(10.13))
bool CompileConcatenation(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                          std::vector<OperationMac>& operations,
                          std::vector<uint32_t>& constants,
                          const OperationMac& concat,
                          const std::map<uint32_t, ValueInfo>& values,
                          const std::unique_ptr<int8_t[]>& memory,
                          const std::vector<OperandMac>& operands);

API_AVAILABLE(macosx(10.13))
bool CompileArithmetic(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                       const OperationMac& operation,
                       const std::vector<OperandMac>& operands,
                       std::vector<uint32_t>& constants,
                       const std::map<uint32_t, ValueInfo>& values,
                       const std::unique_ptr<int8_t[]>& memory);

API_AVAILABLE(macosx(10.13))
bool CompileFullyConnected(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                           OperationMac&,
                           std::vector<OperandMac>& operands,
                           const std::map<uint32_t, ValueInfo>& values,
                           const std::unique_ptr<int8_t[]>& memory);

API_AVAILABLE(macosx(10.13))
bool CompileBilinearScale(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                          OperationMac&,
                          const std::vector<OperandMac>& operands,
                          const std::map<uint32_t, ValueInfo>& values,
                          const std::unique_ptr<int8_t[]>& memory);

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_MAC_MPS_H_
