// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_CL_DNN_H_
#define SERVICES_ML_COMPILATION_DELEGATE_CL_DNN_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/public/interfaces/model.mojom.h"
#include "third_party/clDNN/api/C/cldnn.h"

namespace ml {

class CompilationDelegateClDnn : public CompilationDelegate {
 public:
  explicit CompilationDelegateClDnn(const CompilationImpl*);
  ~CompilationDelegateClDnn() override;

  int32_t Compile() override;
  std::unique_ptr<mojom::Execution> CreateExecution(
      mojom::ExecutionInitParamsPtr params) override;

  static int32_t CldnnGetLayout(int32_t type,
                                const std::vector<uint32_t>& dimensions,
                                cldnn_layout& layout,
                                int32_t format = cldnn_format_bfyx);

 private:
  friend class ExecutionImplClDnn;
  int32_t CldnnInit();
  int32_t CldnnCreateTopology();
  int32_t CldnnCreateProgram();
  int32_t CldnnAddInputLayout(uint32_t index);
  int32_t CldnnAddReorder(const std::string& input_name,
                          const std::string& output_name,
                          int32_t target_format);
  int32_t CldnnAddData(uint32_t index);
  int32_t CldnnAddActivationByFusedCode(const std::string& input,
                                        const std::string& id,
                                        int32_t fuse_code);
  int32_t CldnnAddElementwise(int32_t type,
                              const std::vector<uint32_t>& inputs,
                              const std::vector<uint32_t>& outputs);
  int32_t CldnnAddConvolution(int32_t type,
                              const std::vector<uint32_t>& inputs,
                              const std::vector<uint32_t>& outputs);
  int32_t CldnnAddPooling(int32_t type,
                          const std::vector<uint32_t>& inputs,
                          const std::vector<uint32_t>& outputs);
  int32_t CldnnAddSoftmax(int32_t type,
                          const std::vector<uint32_t>& inputs,
                          const std::vector<uint32_t>& outputs);
  int32_t CldnnAddReshape(int32_t type,
                          const std::vector<uint32_t>& inputs,
                          const std::vector<uint32_t>& outputs);
  int32_t CldnnAddConcatenation(int32_t type,
                                const std::vector<uint32_t>& inputs,
                                const std::vector<uint32_t>& outputs);
  int32_t CldnnAddFullyConnected(int32_t type,
                                 const std::vector<uint32_t>& inputs,
                                 const std::vector<uint32_t>& outputs);
  int32_t CldnnAddResizeBilinear(int32_t type,
                                 const std::vector<uint32_t>& inputs,
                                 const std::vector<uint32_t>& outputs);

 private:
  const CompilationImpl* compilation_;

  cldnn_engine engine_;
  cldnn_topology topology_;
  cldnn_program program_;
  std::vector<cldnn_memory> memories_;

  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateClDnn);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_CL_DNN_H_