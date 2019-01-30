// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_MKL_DNN_H_
#define SERVICES_ML_COMPILATION_DELEGATE_MKL_DNN_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/public/mojom/model.mojom.h"
#include "third_party/mkl-dnn/include/mkldnn.h"

namespace ml {

struct OperationMklDnn {
  explicit OperationMklDnn(mkldnn_primitive_t);
  explicit OperationMklDnn(const mojom::OperationPtr&);
  ~OperationMklDnn();
  OperationMklDnn(const OperationMklDnn&);

  mkldnn_primitive_t primitive;

  // For custom kernels
  int32_t type;
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
};

struct CompiledModelMklDnn {
  CompiledModelMklDnn();
  ~CompiledModelMklDnn();

  std::map<std::string, mkldnn_primitive_t> memories;
  std::vector<OperationMklDnn> operations;
  mkldnn_engine_t engine;
};

class CompilationDelegateMklDnn : public CompilationDelegate {
 public:
  explicit CompilationDelegateMklDnn(const CompilationImpl*);
  ~CompilationDelegateMklDnn() override;

  int32_t Compile() override;
  std::unique_ptr<mojom::Execution> CreateExecution(
      mojom::ExecutionInitParamsPtr params) override;

 private:
  friend class ExecutionImplMklDnn;
  int32_t MkldnnInit();
  int32_t MkldnnCompile();
  int32_t MkldnnGetMemoryFormat(const std::vector<uint32_t>&,
                                mkldnn_memory_format_t*);
  int32_t MkldnnGetDataType(int32_t, mkldnn_data_type_t*);
  int32_t MkldnnGetDims(const std::vector<uint32_t>&,
                        std::vector<int32_t>&,
                        mkldnn_memory_format_t);
  int32_t MkldnnCreateMemoryByQueryType(const mkldnn_primitive_desc_t&,
                                        mkldnn_query_t,
                                        mkldnn_primitive_t&);
  int32_t MkldnnAddMemory(uint32_t index,
                          mkldnn_memory_format_t* format = nullptr);
  int32_t MkldnnAddInput(uint32_t index);
  int32_t MkldnnAddOutput(uint32_t index);
  int32_t MkldnnAddReorder(const std::string& input_name,
                           const std::string& output_name,
                           bool run = false);
  int32_t MkldnnAddFusedActivation(const std::string&,
                                   const std::string&,
                                   int32_t);
  int32_t MkldnnAddElementwise(const mojom::OperationPtr&);
  int32_t MkldnnAddConvolution(const mojom::OperationPtr&);
  int32_t MkldnnAddPooling(const mojom::OperationPtr&);
  int32_t MkldnnAddSoftmax(const mojom::OperationPtr&);
  int32_t MkldnnAddReshape(const mojom::OperationPtr&);
  int32_t MkldnnAddConcatenation(const mojom::OperationPtr&);
  int32_t MkldnnAddFullyConnected(const mojom::OperationPtr&);
  int32_t MkldnnAddResizeBilinear(const mojom::OperationPtr&);

 private:
  const CompilationImpl* compilation_;

  std::shared_ptr<CompiledModelMklDnn> compiled_model_;

  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateMklDnn);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_MKL_DNN_H_