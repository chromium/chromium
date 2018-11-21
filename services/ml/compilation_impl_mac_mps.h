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

namespace ml {

bool CompileConv2DOrDepthwiseConv2D(OperationMac&,
                                    const std::map<uint32_t, ValueInfo>& values,
                                    const std::unique_ptr<int8_t[]>& memory,
                                    const std::vector<OperandMac>& operands);

bool CompileAverageOrMaxPool2D(OperationMac& operation,
                               const std::map<uint32_t, ValueInfo>& values,
                               const std::unique_ptr<int8_t[]>& memory,
                               const std::vector<OperandMac>& operands);

bool CompileSoftmax(OperationMac& operation,
                    const std::map<uint32_t, ValueInfo>& values,
                    const std::unique_ptr<int8_t[]>& memory);

bool CompileReshape(std::vector<OperationMac>& operations,
                    const OperationMac& reshape);

bool CompileConcatenation(std::vector<OperationMac>& operations,
                          const OperationMac& concat,
                          const std::map<uint32_t, ValueInfo>& values,
                          const std::unique_ptr<int8_t[]>& memory,
                          const std::vector<OperandMac>& operands);

bool CompileArithmetic(OperationMac& operation,
                       std::vector<uint32_t>& constants,
                       const std::map<uint32_t, ValueInfo>& values,
                       const std::unique_ptr<int8_t[]>& memory);

bool CompileFullyConnected(OperationMac&,
                           std::vector<OperandMac>& operands,
                           const std::map<uint32_t, ValueInfo>& values,
                           const std::unique_ptr<int8_t[]>& memory);
}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_MAC_MPS_H_
