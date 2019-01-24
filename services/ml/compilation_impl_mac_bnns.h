// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_MAC_BNNS_H_
#define SERVICES_ML_COMPILATION_IMPL_MAC_BNNS_H_

#include <map>
#include <memory>
#include <vector>

#include "services/ml/common.h"
#include "services/ml/ml_utils_mac.h"

namespace ml {
API_AVAILABLE(macosx(10.13))
bool CompileResizeBilinearBNNS(OperationMac& operation);

API_AVAILABLE(macosx(10.13))
bool CompileCompileArithmeticBNNS(OperationMac&,
                                  const std::map<uint32_t, ValueInfo>& values,
                                  const std::unique_ptr<int8_t[]>& memory,
                                  const std::vector<OperandMac>& operands);

API_AVAILABLE(macosx(10.13))
bool CompileConv2DBNNS(OperationMac&,
                       const std::map<uint32_t, ValueInfo>& values,
                       const std::unique_ptr<int8_t[]>& memory,
                       const std::vector<OperandMac>& operands);

API_AVAILABLE(macosx(10.13))
bool CompileAverageOrMaxPool2DBNNS(OperationMac&,
                                   const std::map<uint32_t, ValueInfo>& values,
                                   const std::unique_ptr<int8_t[]>& memory,
                                   const std::vector<OperandMac>& operands);

API_AVAILABLE(macosx(10.13))
bool CompileSoftmaxBNNS(OperationMac&,
                        const std::map<uint32_t, ValueInfo>& values,
                        const std::unique_ptr<int8_t[]>& memory,
                        const std::vector<OperandMac>& operands);

API_AVAILABLE(macosx(10.13))
bool CompileReshapeBNNS(OperationMac&);

API_AVAILABLE(macosx(10.13))
bool CompileConcatenationBNNS(OperationMac& operation,
                              const std::map<uint32_t, ValueInfo>& values,
                              const std::unique_ptr<int8_t[]>& memory,
                              bool is_first_operation);

API_AVAILABLE(macosx(10.13))
bool CompileFullyConnectedBNNS(OperationMac&,
                               const std::map<uint32_t, ValueInfo>& values,
                               const std::unique_ptr<int8_t[]>& memory,
                               const std::vector<OperandMac>& operands);
}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_MAC_BNNS_H_
