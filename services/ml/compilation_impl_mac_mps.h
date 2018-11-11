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
                                    std::map<uint32_t, ValueInfo>& values,
                                    std::unique_ptr<int8_t[]>& memory,
                                    std::vector<OperandMac>& operands);

bool CompileAverageOrMaxPool2D(OperationMac& operation,
                               std::map<uint32_t, ValueInfo>& values,
                               std::unique_ptr<int8_t[]>& memory,
                               std::vector<OperandMac>& operands);

bool CompileSoftmax(OperationMac& operation,
                    std::map<uint32_t, ValueInfo>& values,
                    std::unique_ptr<int8_t[]>& memory);

bool CompileReshape(OperationMac& reshape,
                    std::vector<OperationMac>& operations);

bool CompileConcatenation(OperationMac& concat,
                          std::map<uint32_t, ValueInfo>& values,
                          std::unique_ptr<int8_t[]>& memory,
                          std::vector<OperandMac>& operands,
                          std::vector<OperationMac>& operations);

bool CompileArithmetic(OperationMac& operation,
                       std::map<uint32_t, ValueInfo>& values,
                       std::vector<uint32_t>& constants);

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_MAC_MPS_H_
