// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_IMPL_H_
#define SERVICES_ML_COMPILATION_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "services/ml/common.h"
#include "services/ml/public/mojom/compilation.mojom.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace ml {

class CompilationImpl;
class ModelImpl;

class CompilationDelegate {
 public:
  explicit CompilationDelegate() = default;
  virtual ~CompilationDelegate() = default;

  virtual int32_t Compile() = 0;
  virtual int32_t CreateExecution(
      std::unique_ptr<mojom::Execution>& execution,
      mojom::ExecutionInitParamsPtr params) = 0;
};

struct ElementWiseParams {
  int32_t fuse_code;
};

struct ConvParams {
  bool depthwise;
  bool atrous;
  uint32_t input_batch;
  uint32_t input_height;
  uint32_t input_width;
  uint32_t input_channel;
  uint32_t output_batch;
  uint32_t output_height;
  uint32_t output_width;
  uint32_t output_channel;
  uint32_t filter_height;
  uint32_t filter_width;
  uint32_t bias_length;
  uint32_t depth_in;
  uint32_t depth_out;
  uint32_t padding_left;
  uint32_t padding_right;
  uint32_t padding_top;
  uint32_t padding_bottom;
  uint32_t stride_width;
  uint32_t stride_height;
  uint32_t dilation_width;
  uint32_t dilation_height;
  uint32_t depthwise_multiplier;
  int32_t fuse_code;
};

struct PoolingParams {
  uint32_t input_batch;
  uint32_t input_height;
  uint32_t input_width;
  uint32_t input_channel;
  uint32_t output_batch;
  uint32_t output_height;
  uint32_t output_width;
  uint32_t output_channel;
  uint32_t filter_height;
  uint32_t filter_width;
  uint32_t padding_left;
  uint32_t padding_right;
  uint32_t padding_top;
  uint32_t padding_bottom;
  uint32_t stride_width;
  uint32_t stride_height;
  int32_t fuse_code;
};

struct SoftmaxParams {
  float beta;
};

struct ConcatParams {
  int32_t axis;
};

struct FullyConnectedParams {
  uint32_t input_batch_size;
  uint32_t num_units;
  uint32_t input_size;
  uint32_t bias_num_units;
  uint32_t output_batch_size;
  uint32_t output_num_units;
  int32_t fuse_code;
};

struct ResizeBilinearParams {
  uint32_t height;
  uint32_t width;
  uint32_t new_height;
  uint32_t new_width;
  float y_scale;
  float x_scale;
  bool align_corners;
};

class CompilationImpl : public mojom::Compilation {
 public:
  explicit CompilationImpl(mojom::ModelInfoPtr model_info);
  ~CompilationImpl() override;

  void Finish(int32_t preference, FinishCallback callback) override;
  void CreateExecution(CreateExecutionCallback callback) override;

  const mojom::ModelInfoPtr& GetModel() const { return model_info_; }
  int32_t GetScalarInt32(uint32_t index) const;
  float GetScalarFloat(uint32_t index) const;
  mojo::ScopedSharedBufferMapping MapMemory(uint32_t index) const;
  int32_t GetElementWiseParams(const mojom::OperationPtr&,
                               ElementWiseParams&) const;
  int32_t GetConvParams(const mojom::OperationPtr&, ConvParams&) const;
  int32_t GetPoolingParams(const mojom::OperationPtr&, PoolingParams&) const;
  int32_t GetSoftmaxParams(const mojom::OperationPtr&, SoftmaxParams&) const;
  int32_t GetConcatParams(const mojom::OperationPtr&, ConcatParams&) const;
  int32_t GetFullyConnectedParams(const mojom::OperationPtr&,
                                  FullyConnectedParams&) const;
  int32_t GetResizeBilinearParams(const mojom::OperationPtr&,
                                  ResizeBilinearParams&) const;
  int32_t GetPreference() const { return preference_; }

 private:
  int32_t preference_;
  mojom::ModelInfoPtr model_info_;

  std::unique_ptr<CompilationDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CompilationImpl);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_IMPL_H_