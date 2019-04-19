// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_DML_H_
#define SERVICES_ML_COMPILATION_DELEGATE_DML_H_

#include <memory>

#include "base/macros.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace ml {

class CompilationDelegateDML : public CompilationDelegate {
 public:
  explicit CompilationDelegateDML(const CompilationImpl*);
  ~CompilationDelegateDML() override;

  int32_t Compile() override;
  int32_t CreateExecution(std::unique_ptr<mojom::Execution>& execution,
                          mojom::ExecutionInitParamsPtr params) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateDML);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_DML_H_