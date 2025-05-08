// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUBGROUP_MATRIX_CONFIG_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUBGROUP_MATRIX_CONFIG_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_subgroup_matrix_component_type.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"

namespace blink {

class GPUSubgroupMatrixConfig : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUSubgroupMatrixConfig(const wgpu::SubgroupMatrixConfig&);
  GPUSubgroupMatrixConfig(const GPUSubgroupMatrixConfig&) = delete;
  GPUSubgroupMatrixConfig& operator=(const GPUSubgroupMatrixConfig&) = delete;

  V8GPUSubgroupMatrixComponentType componentType() const;
  V8GPUSubgroupMatrixComponentType resultComponentType() const;
  uint64_t M() const { return config_.M; }
  uint64_t N() const { return config_.N; }
  uint64_t K() const { return config_.K; }

 private:
  wgpu::SubgroupMatrixConfig config_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_SUBGROUP_MATRIX_CONFIG_H_
