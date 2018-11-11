// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_ML_UTILS_MAC_H_
#define SERVICES_ML_ML_UTILS_MAC_H_

// The header file can't be included, otherwise the declaration of
// MPSCNNKernel will be used.
// #import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import <Accelerate/Accelerate.h>
#include <map>
#include <memory>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "services/ml/common.h"

@class MPSCNNKernel;
@class MPSCNNBinaryKernel;

typedef enum LocalOperation {
  KBNNSFilter = 1,
  KReshape = 2,
  KConcatenation = 3,
} LocalOperation;

namespace ml {

struct OperandMac : public Operand {
  OperandMac();
  explicit OperandMac(const OperandMac&);
  explicit OperandMac(const Operand&);
  ~OperandMac();
  uint32_t read_count;
};

struct OperationMac : public Operation {
  OperationMac();
  explicit OperationMac(const OperationMac&);
  explicit OperationMac(const Operation&);
  ~OperationMac();
  base::scoped_nsobject<MPSCNNKernel> mpscnn_kernel;
  base::scoped_nsobject<MPSCNNBinaryKernel> mpscnn_binary_kernel;
  ::BNNSFilter filter;
  LocalOperation local_operation;

  int fuse_code;
  uint32_t offset_x;
  uint32_t offset_y;
  std::vector<float*> concatenations;
};

bool ParameterExtracterForConv(const OperationMac&,
                               const std::vector<uint32_t>&,
                               const std::vector<uint32_t>&,
                               std::map<uint32_t, ValueInfo>& values,
                               std::unique_ptr<int8_t[]>& memory,
                               std::vector<OperandMac>& operands,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               bool&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               int32_t&,
                               bool depthwise = false);

}  // namespace ml

#endif  // SERVICES_ML_ML_UTILS_MAC_H_