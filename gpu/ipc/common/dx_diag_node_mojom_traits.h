// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_DX_DIAG_NODE_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_DX_DIAG_NODE_MOJOM_TRAITS_H_

#include "gpu/ipc/common/dx_diag_node.mojom.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::DxDiagNodeDataView, gpu::DxDiagNode> {
  static bool Read(gpu::mojom::DxDiagNodeDataView data, gpu::DxDiagNode* out);

  static const std::map<std::string, std::string>& values(
      const gpu::DxDiagNode& node) {
    return node.values;
  }

  static const std::map<std::string, gpu::DxDiagNode>& children(
      const gpu::DxDiagNode& node) {
    return node.children;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_DX_DIAG_NODE_MOJOM_TRAITS_H_
