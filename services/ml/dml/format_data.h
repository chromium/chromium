// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_ML_DML_FORMAT_DATA_H_
#define SERVICES_ML_DML_FORMAT_DATA_H_

#include <wrl/client.h>
#include "base/macros.h"
#include "d3d12.h"

using Microsoft::WRL::ComPtr;

namespace ml {

class CompiledModelDML;

class FormatData {
 public:
  explicit FormatData(CompiledModelDML* dml);
  ~FormatData();

  HRESULT FormatInputData(CompiledModelDML* dml);
  HRESULT FormatOutputData(CompiledModelDML* dml);

 private:
  ComPtr<ID3D12DescriptorHeap> descriptor_heap_;
  // Compute objects for formatting input/output data.
  ComPtr<ID3D12PipelineState> input_pipline_state_;
  ComPtr<ID3D12PipelineState> output_pipline_state_;
  ComPtr<ID3D12RootSignature> root_signature_;

  DISALLOW_COPY_AND_ASSIGN(FormatData);
};

}  // namespace ml

#endif  // SERVICES_ML_DML_FORMAT_DATA_H_