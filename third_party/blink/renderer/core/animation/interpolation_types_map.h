// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_TYPES_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_TYPES_MAP_H_

#include <memory>
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class InterpolationType;
class PropertyHandle;

using InterpolationTypes = Vector<std::unique_ptr<const InterpolationType>>;

class InterpolationTypesMap {
  STACK_ALLOCATED();

 public:
  virtual const InterpolationTypes& Get(const PropertyHandle&) const = 0;
  virtual size_t Version() const { return 0; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLATION_TYPES_MAP_H_
