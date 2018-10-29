// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/dx_diag_node.h"

namespace gpu {

DxDiagNode::DxDiagNode() = default;

DxDiagNode::DxDiagNode(const DxDiagNode& other) = default;

DxDiagNode::~DxDiagNode() = default;

bool DxDiagNode::IsEmpty() const {
  return values.empty() && children.empty();
}

}  // namespace gpu
