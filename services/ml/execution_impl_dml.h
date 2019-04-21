// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef SERVICES_ML_EXECUTION_IMPL_DML_H_
#define SERVICES_ML_EXECUTION_IMPL_DML_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "services/ml/ml_utils_dml.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace ml {

class CompilationDelegateDML;

class ExecutionImplDML : public mojom::Execution {
 public:
  ExecutionImplDML(const CompilationDelegateDML*,
                   std::unique_ptr<ExecutionData> dml,
                   mojom::ExecutionInitParamsPtr params);
  ~ExecutionImplDML() override;

  void StartCompute(StartComputeCallback callback) override;

 private:
  HRESULT ExecuteCompiledOperator(IDMLCompiledOperator*,
                                  uint32_t,
                                  const mojom::OperationPtr&);
  HRESULT ReadResultBack(uint32_t memory_offset);

  const CompilationDelegateDML* compilation_;
  mojom::ExecutionInitParamsPtr params_;
  std::unique_ptr<ExecutionData> dml_;
  std::map<uint32_t, ComPtr<ID3D12Resource>> input_resources_;
  std::map<uint32_t, ComPtr<ID3D12Resource>> upload_resources_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplDML);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_DML_H_