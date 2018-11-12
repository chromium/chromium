// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_MAC_MPS_H_
#define SERVICES_ML_EXECUTION_IMPL_MAC_MPS_H_

#import <Metal/MTLBuffer.h>
#import <Metal/MTLCommandBuffer.h>
#include <map>
#include <memory>
#include <vector>

#include "base/mac/availability.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl_mac.h"
#include "services/ml/public/interfaces/execution.mojom.h"

@class MPSImage;
@class MPSTemporaryImage;

namespace ml {

class ExecutionImplMacMPS : public mojom::Execution {
 public:
  ExecutionImplMacMPS(base::WeakPtr<CompilationImplMac>,
                      mojo::ScopedSharedBufferHandle);
  ~ExecutionImplMacMPS() override;

  void StartCompute(StartComputeCallback callback) override;

  bool IsValid() const;

 private:
  void API_AVAILABLE(macos(10_13))
      SetupMPSImageForOperands(std::vector<base::scoped_nsobject<MPSImage>>&,
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

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplMacMPS);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_MAC_MPS_H_
