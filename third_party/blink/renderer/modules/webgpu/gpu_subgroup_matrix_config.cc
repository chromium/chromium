// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_subgroup_matrix_config.h"

#include "base/notreached.h"

namespace blink {

namespace {
V8GPUSubgroupMatrixComponentType::Enum FromDawnEnum(
    wgpu::SubgroupMatrixComponentType type) {
  switch (type) {
    case wgpu::SubgroupMatrixComponentType::F16:
      return V8GPUSubgroupMatrixComponentType::Enum::kF16;
    case wgpu::SubgroupMatrixComponentType::F32:
      return V8GPUSubgroupMatrixComponentType::Enum::kF32;
    case wgpu::SubgroupMatrixComponentType::I32:
      return V8GPUSubgroupMatrixComponentType::Enum::kI32;
    case wgpu::SubgroupMatrixComponentType::U32:
      return V8GPUSubgroupMatrixComponentType::Enum::kU32;
    case wgpu::SubgroupMatrixComponentType::I8:
      return V8GPUSubgroupMatrixComponentType::Enum::kI8;
    case wgpu::SubgroupMatrixComponentType::U8:
      return V8GPUSubgroupMatrixComponentType::Enum::kU8;
    default:
      NOTREACHED();
  }
}

}  // namespace

GPUSubgroupMatrixConfig::GPUSubgroupMatrixConfig(
    const wgpu::SubgroupMatrixConfig& config)
    : config_(config) {}

V8GPUSubgroupMatrixComponentType GPUSubgroupMatrixConfig::componentType()
    const {
  return V8GPUSubgroupMatrixComponentType(FromDawnEnum(config_.componentType));
}
V8GPUSubgroupMatrixComponentType GPUSubgroupMatrixConfig::resultComponentType()
    const {
  return V8GPUSubgroupMatrixComponentType(
      FromDawnEnum(config_.resultComponentType));
}

}  // namespace blink
