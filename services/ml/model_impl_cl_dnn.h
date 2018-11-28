// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MODEL_IMPL_CL_DNN_H_
#define SERVICES_ML_MODEL_IMPL_CL_DNN_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#if defined(OS_LINUX)
#include "services/ml/cl_dnn_symbol_table.h"
#endif
#include "services/ml/common.h"
#include "services/ml/public/interfaces/model.mojom.h"
#include "third_party/clDNN/api/C/cldnn.h"

// Accesses clDNN functions through our late-binding symbol table instead of
// directly. This way we don't have to link to libclDNN, which means our binary
// will work on systems that don't have it.
#if defined(OS_LINUX)
extern ml::ClDnnSymbolTable* GetClDnnSymbolTable();
#define LATE(sym) LATESYM_GET(ml::ClDnnSymbolTable, GetClDnnSymbolTable(), sym)
#else
#define LATE(sym) sym
#endif

namespace ml {

class CompilationImplClDnn;

class ModelImplClDnn : public mojom::Model {
 public:
  ModelImplClDnn();
  ~ModelImplClDnn() override;

  bool IsValid();

  void Finish(mojom::ModelInfoPtr model_info, FinishCallback callback) override;

  void CreateCompilation(CreateCompilationCallback callback) override;

 private:
  int32_t AddOperand(int32_t type,
                     const std::vector<uint32_t>& dimensions,
                     float scale,
                     int32_t zeroPoint);
  int32_t SetOperandValue(uint32_t index, const void* buffer, uint32_t length);
  int32_t AddOperation(int32_t type,
                       const std::vector<uint32_t>& inputs,
                       const std::vector<uint32_t>& outputs);
  int32_t IdentifyInputsAndOutputs(const std::vector<uint32_t>& inputs,
                                   const std::vector<uint32_t>& outputs);

  int32_t CldnnGetLayout(const Operand& operand,
                         cldnn_layout& layout,
                         int32_t format = cldnn_format_bfyx);
  int32_t CldnnAddInputLayout(uint32_t index);
  int32_t CldnnAddReorderForOperand(int32_t index, int32_t target_format);
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
  friend class CompilationImplClDnn;

  std::vector<Operand> operands_;
  std::vector<Operation> operations_;
  std::map<uint32_t, ValueInfo> values_;
  std::vector<uint32_t> inputs_;
  std::vector<uint32_t> outputs_;
  std::unique_ptr<int8_t[]> memory_;
  uint32_t memory_size_;

  cldnn_engine engine_;
  cldnn_topology topology_;
  std::vector<cldnn_memory> memories_;

  DISALLOW_COPY_AND_ASSIGN(ModelImplClDnn);
};

}  // namespace ml

#endif  // SERVICES_ML_MODEL_IMPL_CL_DNN_H_