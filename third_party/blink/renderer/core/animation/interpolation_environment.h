// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_ENVIRONMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_ENVIRONMENT_H_

#include "third_party/blink/renderer/core/animation/interpolation_types_map.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class InterpolationEnvironment {
  STACK_ALLOCATED();
 public:
  virtual bool IsCSS() const { return false; }
  virtual bool IsSVG() const { return false; }

  const InterpolationTypesMap& GetInterpolationTypesMap() const {
    return interpolation_types_map_;
  }

 protected:
  virtual ~InterpolationEnvironment() = default;

  explicit InterpolationEnvironment(const InterpolationTypesMap& map)
      : interpolation_types_map_(map) {}

 private:
  const InterpolationTypesMap& interpolation_types_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_ENVIRONMENT_H_
