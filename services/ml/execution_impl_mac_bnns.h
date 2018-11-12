// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_EXECUTION_IMPL_MAC_BNNS_H_
#define SERVICES_ML_EXECUTION_IMPL_MAC_BNNS_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl_mac.h"
#include "services/ml/public/interfaces/execution.mojom.h"

namespace ml {

class ExecutionImplMacBNNS : public mojom::Execution {
 public:
  ExecutionImplMacBNNS(base::WeakPtr<CompilationImplMac>,
                       mojo::ScopedSharedBufferHandle);
  ~ExecutionImplMacBNNS() override;

  void PrepareBnnsOperandsMemory();
  void StartCompute(StartComputeCallback callback) override;

  bool IsValid() const;

 private:
  base::WeakPtr<CompilationImplMac> compilation_;

  std::vector<std::unique_ptr<OperandInfo>> inputs_info_;
  std::vector<std::unique_ptr<OperandInfo>> outputs_info_;
  std::map<size_t, float*> bnns_operands_memory_map_;

  DISALLOW_COPY_AND_ASSIGN(ExecutionImplMacBNNS);
};

}  // namespace ml

#endif  // SERVICES_ML_EXECUTION_IMPL_MAC_BNNS_H_
