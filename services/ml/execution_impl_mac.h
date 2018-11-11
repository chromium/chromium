// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_MAC_H_
#define SERVICES_ML_EXECUTION_IMPL_MAC_H_

#import <Metal/MTLBuffer.h>
#import <Metal/MTLCommandBuffer.h>
#include <math.h>
#include <map>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl_mac.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/model_impl_mac.h"
#include "services/ml/public/interfaces/constants.mojom.h"
#include "services/ml/public/interfaces/execution.mojom.h"

@class MPSImage;
@class MPSTemporaryImage;

namespace ml {

class ExecutionImplMac : public mojom::Execution {
 public:
  ExecutionImplMac(base::WeakPtr<CompilationImplMac>,
                   mojo::ScopedSharedBufferHandle);
  ~ExecutionImplMac() override;

  void PrepareBnnsOperandsMemory();
  void StartCompute(StartComputeCallback callback) override;

  bool IsValid() const;

 private:
  void SetupOperandInfoForOperands(
    std::vector<std::unique_ptr<OperandInfo>>&,
    const std::vector<uint32_t>&);
  void API_AVAILABLE(macos(10_13)) SetupMPSImageForOperands(
    std::vector<base::scoped_nsobject<MPSImage>>&,
    std::vector<id<MTLBuffer>>&,
    const std::vector<uint32_t>&);

  MPSImage* API_AVAILABLE(macos(10_13))
      FindInputOrConstantMPSImageByIndex(uint32_t);
  MPSImage* API_AVAILABLE(macos(10_13)) FindOutputMPSImageByIndex(uint32_t);
  MPSTemporaryImage* API_AVAILABLE(macos(10_13))
      FindOrCreateMPSTemporaryImageByIndex(uint32_t,
                                           const id<MTLCommandBuffer>&);
  void API_AVAILABLE(macos(10_13)) UploadToMPSImage(const MPSImage*,
                                                    const id<MTLBuffer>&,
                                                    const id<MTLCommandBuffer>&,
                                                    const void*,
                                                    size_t);

  base::WeakPtr<CompilationImplMac> compilation_;

  std::vector<std::unique_ptr<OperandInfo>> inputs_info_;
  std::vector<std::unique_ptr<OperandInfo>> outputs_info_;
  std::map<size_t, float*> bnns_operands_memory_map_;
  mojo::ScopedSharedBufferHandle memory_;
  uint32_t mapped_length_;
  uint32_t is_bnns_;

  API_AVAILABLE(macos(10_13))
  std::vector<base::scoped_nsobject<MPSImage>> input_mpsimages_;
  API_AVAILABLE(macos(10_13)) std::vector<id<MTLBuffer>> input_mtlbuffers_;
  API_AVAILABLE(macos(10_13))
  std::vector<base::scoped_nsobject<MPSImage>> output_mpsimages_;
  API_AVAILABLE(macos(10_13)) std::vector<id<MTLBuffer>> output_mtlbuffers_;
  API_AVAILABLE(macos(10_13))
  std::vector<base::scoped_nsobject<MPSImage>> constant_mpsimages_;
  API_AVAILABLE(macos(10_13)) std::vector<id<MTLBuffer>> constant_mtlbuffers_;
  API_AVAILABLE(macos(10_13))
  std::map<uint32_t, MPSTemporaryImage*> tmp_mpsimage_cache_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplMac);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_MAC_H_
