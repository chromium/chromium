// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_IE_H_
#define SERVICES_ML_COMPILATION_DELEGATE_IE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace InferenceEngine {
class CNNNetwork;
class Blob;
namespace Builder {
class Network;
}
}  // namespace InferenceEngine

namespace ml {

class CompilationDelegateIe : public CompilationDelegate {
 public:
  explicit CompilationDelegateIe(const CompilationImpl*);
  ~CompilationDelegateIe() override;

  int32_t Compile() override;
  int32_t CreateExecution(std::unique_ptr<mojom::Execution>& execution,
                          mojom::ExecutionInitParamsPtr params) override;

 private:
  friend class ExecutionImplIe;
  template<typename T>
  static int32_t Reorder(T* dst,
                         const float* src,
                         std::vector<uint32_t>& dims,
                         bool nhwc_to_nchw = true);
  static int32_t GetDims(const std::vector<uint32_t>&, std::vector<size_t>&);
  int32_t Init();
  int32_t BuildNetwork();
  int32_t CreateBlob(uint32_t index,
                     std::shared_ptr<InferenceEngine::Blob>& blob);
  int32_t AddInput(uint32_t index);
  int32_t AddOutput(uint32_t index);
  int32_t AddConstant(uint32_t index);
  int32_t AddActivationByFusedCode(int32_t fuse_code,
                                   size_t input_layer,
                                   const std::string& name,
                                   size_t& activiation_layer_id);
  int32_t AddElementwise(const mojom::OperationPtr& operation);
  int32_t AddConvolution(const mojom::OperationPtr& operation);
  int32_t AddPooling(const mojom::OperationPtr& operation);
  int32_t AddSoftmax(const mojom::OperationPtr& operation);
  int32_t AddReshape(const mojom::OperationPtr& operation);
  int32_t AddConcatenation(const mojom::OperationPtr& operation);
  int32_t AddFullyConnected(const mojom::OperationPtr& operation);
  int32_t AddResizeBilinear(const mojom::OperationPtr& operation);

 private:
  const CompilationImpl* compilation_;

  std::unique_ptr<InferenceEngine::Builder::Network> builder_;
  std::unique_ptr<InferenceEngine::CNNNetwork> network_;

  std::map<uint32_t, size_t> layer_id_map_;

  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateIe);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_IE_H_