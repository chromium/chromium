// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_BNNS_H_
#define SERVICES_ML_COMPILATION_DELEGATE_BNNS_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/execution_impl_bnns.h"
#include "services/ml/ml_utils_mac.h"
#include "services/ml/public/mojom/compilation.mojom.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace ml {

class CompiledModelBnns : public CompiledModel,
                          public base::RefCounted<CompiledModelBnns> {
 public:
  CompiledModelBnns();
  std::vector<uint32_t> constants_;
 private:
  friend class base::RefCounted<CompiledModelBnns>;
  ~CompiledModelBnns();
};

class API_AVAILABLE(macosx(10.13)) CompilationDelegateBnns
    : public CompilationDelegate {
 public:
  explicit CompilationDelegateBnns(const CompilationImpl*);
  ~CompilationDelegateBnns() override;

  int32_t Compile() override;
  int32_t CreateExecution(std::unique_ptr<mojom::Execution>& execution,
                          mojom::ExecutionInitParamsPtr params) override;

 private:
  const CompilationImpl* compilation_;
  scoped_refptr<CompiledModelBnns> compiled_model_;

  bool CompileConvolution(const mojom::ModelInfoPtr& model,
                          const mojom::OperationPtr& operation,
                          OperationMac& operation_bnns);

  bool CompileArithmetic(const mojom::ModelInfoPtr& model,
                         const mojom::OperationPtr& operation,
                         OperationMac& operation_bnns);

  bool CompilePooling(const mojom::ModelInfoPtr& model,
                      const mojom::OperationPtr& operation,
                      OperationMac& operation_bnns);

  bool CompileSoftmax(const mojom::ModelInfoPtr& model,
                      const mojom::OperationPtr& operation,
                      OperationMac& operation_bnns);

  bool CompileReshape(const mojom::ModelInfoPtr& model,
                      const mojom::OperationPtr& operation,
                      OperationMac& operation_bnns);


  bool CompileConcatenation(const mojom::ModelInfoPtr& model,
                            const mojom::OperationPtr& operation,
                            OperationMac& operation_bnns);

  bool CompileFullyConnected(const mojom::ModelInfoPtr& model,
                             const mojom::OperationPtr& operation,
                             OperationMac& operation_bnns);

  bool CompileBilinearScale(const mojom::ModelInfoPtr& model,
                            const mojom::OperationPtr& operation,
                            OperationMac& operation_bnns);

  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateBnns);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_BNNS_H_