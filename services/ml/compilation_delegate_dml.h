// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_DML_H_
#define SERVICES_ML_COMPILATION_DELEGATE_DML_H_

#include <DirectML.h>
#include <wrl/client.h>
#include <memory>

#include "base/macros.h"
#include "d3d12.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/execution_impl_dml.h"
#include "services/ml/public/mojom/model.mojom.h"

using Microsoft::WRL::ComPtr;

namespace ml {

class CompilationDelegateDML : public CompilationDelegate {
 public:
  explicit CompilationDelegateDML(const CompilationImpl*);
  ~CompilationDelegateDML() override;

  int32_t Compile() override;
  int32_t CreateExecution(std::unique_ptr<mojom::Execution>& execution,
                          mojom::ExecutionInitParamsPtr params) override;
  const mojom::ModelInfoPtr& GetModel() const;
  mojo::ScopedSharedBufferMapping MapMemory(uint32_t index) const;

 private:
  const CompilationImpl* compilation_;

  friend class ExecutionImplDML;
  HRESULT InitializeOperators();
  HRESULT CompileArithmetic(const mojom::ModelInfoPtr& model,
                            const mojom::OperationPtr& operation,
                            std::vector<uint32_t>& constants);

  std::unique_ptr<ExecutionData> dml_;
  UINT execute_descriptor_count_;
  uint64_t execute_temporary_resource_size_;

  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateDML);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_DML_H_