// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_RESULT_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"

namespace blink {

class TransformationMatrix;

class XRHitResult final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRHitResult(const TransformationMatrix&);
  ~XRHitResult() override;

  DOMFloat32Array* hitMatrix() const;

 private:
  const std::unique_ptr<TransformationMatrix> hit_transform_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HIT_RESULT_H_
