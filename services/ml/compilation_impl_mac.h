// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_MAC_H_
#define SERVICES_ML_COMPILATION_IMPL_MAC_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/model_impl_mac.h"
#include "services/ml/public/interfaces/compilation.mojom.h"

@class MPSNNGraph;
@class MPSNNImageNode;

namespace ml {

class ExecutionImplMac;

class CompilationImplMac : public mojom::Compilation {
 public:
  explicit CompilationImplMac(ModelImplMac*);
  ~CompilationImplMac() override;

  void Finish(int32_t preference, FinishCallback callback) override;
  void CreateExecution(CreateExecutionCallback callback) override;

 private:
  void CompileModelWithBNNS(FinishCallback callback);
  void CompileModelWithMPS(FinishCallback callback);
  friend class ExecutionImplMacBNNS;
  friend class ExecutionImplMacMPS;

  std::vector<OperandMac> operands_;
  std::vector<OperationMac> operations_;
  std::map<uint32_t, ValueInfo> values_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;
  std::vector<uint32_t> constants_;
  std::unique_ptr<int8_t[]> memory_;
  uint32_t memory_size_;
  bool is_bnns_;

  // Used for MPSNNGraph
  std::vector<base::scoped_nsobject<MPSNNGraph>> graphs_;
  std::map<uint32_t, MPSNNImageNode*> mps_image_nodes_;

  base::WeakPtrFactory<CompilationImplMac> compilation_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompilationImplMac);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_MAC_H_
