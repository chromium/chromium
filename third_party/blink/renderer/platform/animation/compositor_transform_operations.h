// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_OPERATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_OPERATIONS_H_

#include "cc/animation/transform_operations.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

class SkMatrix44;

namespace blink {

class PLATFORM_EXPORT CompositorTransformOperations {
  STACK_ALLOCATED();

 public:
  const cc::TransformOperations& AsCcTransformOperations() const;
  cc::TransformOperations ReleaseCcTransformOperations();

  // Returns true if these operations can be blended. It will only return
  // false if we must resort to matrix interpolation, and matrix interpolation
  // fails (this can happen if either matrix cannot be decomposed).
  bool CanBlendWith(const CompositorTransformOperations& other) const;

  void AppendTranslate(double x, double y, double z);
  void AppendRotate(double x, double y, double z, double degrees);
  void AppendScale(double x, double y, double z);
  void AppendSkew(double x, double y);
  void AppendPerspective(double depth);
  void AppendMatrix(const SkMatrix44&);
  void AppendIdentity();

  bool IsIdentity() const;

 private:
  cc::TransformOperations transform_operations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_OPERATIONS_H_
