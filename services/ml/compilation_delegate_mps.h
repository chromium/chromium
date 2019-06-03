// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_MPS_H_
#define SERVICES_ML_COMPILATION_DELEGATE_MPS_H_

#include <map>
#include <memory>
#include <vector>

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/execution_impl_mps.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/public/mojom/compilation.mojom.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace ml {

class API_AVAILABLE(macosx(10.13)) CompiledModelMPS
    : public CompiledModel,
      public base::RefCounted<CompiledModelMPS> {
 public:
  CompiledModelMPS();

  std::vector<uint32_t> constants_;
  std::map<std::string, ValueInfo> values_;
  std::unique_ptr<int8_t[]> memory_;
  std::vector<base::scoped_nsobject<MPSNNGraph>> graphs_;
  std::map<uint32_t, MPSNNImageNode*> mps_image_nodes_;

 private:
  friend class base::RefCounted<CompiledModelMPS>;
  ~CompiledModelMPS();

  DISALLOW_COPY_AND_ASSIGN(CompiledModelMPS);
};

class API_AVAILABLE(macosx(10.13)) CompilationDelegateMPS
    : public CompilationDelegate {
 public:
  explicit CompilationDelegateMPS(const CompilationImpl*);
  ~CompilationDelegateMPS() override;

  int32_t Compile() override;
  int32_t CreateExecution(std::unique_ptr<mojom::Execution>& execution,
                          mojom::ExecutionInitParamsPtr params) override;

 private:
  const CompilationImpl* compilation_;
  scoped_refptr<CompiledModelMPS> compiled_model_;

  bool CompileConv2DOrDepthwiseConv2D(
      std::map<uint32_t, MPSNNImageNode*>& image_nodes,
      const mojom::ModelInfoPtr& model,
      const mojom::OperationPtr& operation);

  bool CompileArithmetic(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                         const mojom::ModelInfoPtr& model,
                         const mojom::OperationPtr& operation);

  bool CompileAverageOrMaxPool2D(
      std::map<uint32_t, MPSNNImageNode*>& image_nodes,
      const mojom::ModelInfoPtr& model,
      const mojom::OperationPtr& operation);

  bool CompileSoftmax(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                      const mojom::ModelInfoPtr& model,
                      const mojom::OperationPtr& operation);

  bool CompileReshape(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                      const mojom::ModelInfoPtr& model,
                      const mojom::OperationPtr& operation);

  bool CompileConcatenation(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                            const mojom::ModelInfoPtr& model,
                            const mojom::OperationPtr& operation);

  bool CompileFullyConnected(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                             const mojom::ModelInfoPtr& model,
                             const mojom::OperationPtr& operation);

  bool CompileBilinearScale(std::map<uint32_t, MPSNNImageNode*>& image_nodes,
                            const mojom::ModelInfoPtr& model,
                            const mojom::OperationPtr& operation);

  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateMPS);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_MPS_H_