// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_MPS_H_
#define SERVICES_ML_EXECUTION_IMPL_MPS_H_

#import <Metal/MTLBuffer.h>
#import <Metal/MTLCommandBuffer.h>
#include <map>
#include <memory>
#include <vector>

#include "base/mac/availability.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/ml/common.h"
// #include "services/ml/compilation_impl_mac.h"
#include "services/ml/compilation_delegate_mps.h"
#include "services/ml/public/mojom/execution.mojom.h"

@class MPSImage;
@class MPSTemporaryImage;

namespace ml {

class CompilationDelegateMPS;
class CompiledModelMPS;

class API_AVAILABLE(macosx(10.13)) ExecutionImplMPS : public mojom::Execution {
 public:
  ExecutionImplMPS(scoped_refptr<CompiledModelMPS> compiled_model,
                   mojom::ExecutionInitParamsPtr params);
  ~ExecutionImplMPS() override;

  void StartCompute(StartComputeCallback callback) override;

 private:
  mojom::ExecutionInitParamsPtr params_;

  scoped_refptr<CompiledModelMPS> compiled_model_;

  void ReadResultBack(uint32_t memory_offset);

  std::vector<id<MTLBuffer>> output_mtlbuffers_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplMPS);
};

}  // namespace ml

#endif