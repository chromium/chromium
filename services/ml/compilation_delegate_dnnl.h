// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_DNNL_H_
#define SERVICES_ML_COMPILATION_DELEGATE_DNNL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/public/mojom/model.mojom.h"
#include "third_party/dnnl/include/dnnl.h"

namespace ml {

typedef std::vector<dnnl_exec_arg_t> args_t;
struct OperationDnnl {
  OperationDnnl();
  explicit OperationDnnl(dnnl_primitive_t);
  explicit OperationDnnl(const mojom::OperationPtr&);
  ~OperationDnnl();
  OperationDnnl(const OperationDnnl&);

  dnnl_primitive_t primitive;
  args_t primitive_args;

  // For custom kernels
  int32_t type;
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
};

struct CompiledModelDnnl {
  CompiledModelDnnl();
  ~CompiledModelDnnl();

  std::map<std::string, dnnl_memory_t> memories;
  std::vector<OperationDnnl> operations;
  dnnl_engine_t engine;
};

class CompilationDelegateDnnl : public CompilationDelegate {
 public:
  explicit CompilationDelegateDnnl(const CompilationImpl*);
  ~CompilationDelegateDnnl() override;

  int32_t Compile() override;
  int32_t CreateExecution(std::unique_ptr<mojom::Execution>& execution,
                          mojom::ExecutionInitParamsPtr params) override;

 private:
  friend class ExecutionImplDnnl;
  int32_t DnnlInit();
  int32_t DnnlCompile();
  int32_t GetMemoryFormat(const std::vector<uint32_t>&, dnnl_format_tag_t*);
  int32_t GetDataType(int32_t, dnnl_data_type_t*);
  int32_t GetDims(const std::vector<uint32_t>&,
                  std::vector<dnnl_dim_t>&,
                  dnnl_format_tag_t);
  int32_t CreateMemoryDescriptor(int32_t,
                                 dnnl_memory_desc_t&,
                                 dnnl_format_tag_t* format = nullptr);
  int32_t CreateMemoryByQueryType(const dnnl_primitive_desc_t&,
                                  dnnl_query_t,
                                  dnnl_memory_t&);
  int32_t CreateMemory(uint32_t index,
                       dnnl_memory_t&,
                       dnnl_format_tag_t* format = nullptr);
  int32_t AddInput(uint32_t index);
  int32_t AddOutput(uint32_t index);
  int32_t AddReorder(const std::string& input_name,
                     const std::string& output_name,
                     bool run = false);
  int32_t AddFusedActivation(const std::string&, const std::string&, int32_t);
  int32_t AddElementwise(const mojom::OperationPtr&);
  int32_t AddConvolution(const mojom::OperationPtr&);
  int32_t AddPooling(const mojom::OperationPtr&);
  int32_t AddSoftmax(const mojom::OperationPtr&);
  int32_t AddReshape(const mojom::OperationPtr&);
  int32_t AddConcatenation(const mojom::OperationPtr&);
  int32_t AddFullyConnected(const mojom::OperationPtr&);
  int32_t AddResizeBilinear(const mojom::OperationPtr&);

 private:
  const CompilationImpl* compilation_;

  std::shared_ptr<CompiledModelDnnl> compiled_model_;

  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateDnnl);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_DNNL_H_
