// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hit_result.h"

#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {
XRHitResult::XRHitResult(const TransformationMatrix& hit_transform)
    : hit_transform_(std::make_unique<TransformationMatrix>(hit_transform)) {}

XRHitResult::~XRHitResult() {}

DOMFloat32Array* XRHitResult::hitMatrix() const {
  if (!hit_transform_)
    return nullptr;
  return transformationMatrixToDOMFloat32Array(*hit_transform_);
}

}  // namespace blink
