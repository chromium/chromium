/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_GEOMETRY_MAP_STEP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_GEOMETRY_MAP_STEP_H_

#include <memory>
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;

enum GeometryInfoFlag {
  kAccumulatingTransform = 1 << 0,
  kIsNonUniform =
      1
      << 1,  // Mapping depends on the input point, e.g. because of CSS columns.
  kIsFixedPosition = 1 << 2,
  kContainsFixedPosition = 1 << 3,
};
typedef unsigned GeometryInfoFlags;

// Stores data about how to map from one layoutObject to its container.
struct LayoutGeometryMapStep {
  DISALLOW_NEW();
  LayoutGeometryMapStep(const LayoutGeometryMapStep& o)
      : layout_object_(o.layout_object_),
        offset_(o.offset_),
        offset_for_fixed_position_(o.offset_for_fixed_position_),
        flags_(o.flags_) {
    DCHECK(!o.transform_);
  }
  LayoutGeometryMapStep(const LayoutObject* layout_object,
                        GeometryInfoFlags flags)
      : layout_object_(layout_object), flags_(flags) {}
  const LayoutObject* layout_object_;
  PhysicalOffset offset_;
  std::unique_ptr<TransformationMatrix>
      transform_;  // Includes offset if non-null.
  PhysicalOffset offset_for_fixed_position_;
  GeometryInfoFlags flags_;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::LayoutGeometryMapStep)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_GEOMETRY_MAP_STEP_H_
