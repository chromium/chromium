// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_EXCLUSION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_EXCLUSION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_rect.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class LayoutBox;

struct CORE_EXPORT NGExclusionShapeData final
    : public GarbageCollected<NGExclusionShapeData> {
  NGExclusionShapeData(const LayoutBox* layout_box,
                       const NGBoxStrut& margins,
                       const NGBoxStrut& shape_insets)
      : layout_box(layout_box), margins(margins), shape_insets(shape_insets) {}
  NGExclusionShapeData(const NGExclusionShapeData& other)
      : layout_box(other.layout_box),
        margins(other.margins),
        shape_insets(other.shape_insets) {}

  void Trace(Visitor*) const;

  Member<const LayoutBox> layout_box;
  const NGBoxStrut margins;
  const NGBoxStrut shape_insets;
};

// Struct that represents an exclusion. This currently is just a float but
// we've named it an exclusion to potentially support other types in the future.
struct CORE_EXPORT NGExclusion final : public GarbageCollected<NGExclusion> {
  NGExclusion(const NGBfcRect& rect,
              const EFloat type,
              const NGExclusionShapeData* shape_data)
      : rect(rect), type(type), shape_data(std::move(shape_data)) {}

  static const NGExclusion* Create(const NGBfcRect& rect,
                                   const EFloat type,
                                   NGExclusionShapeData* shape_data = nullptr) {
    return MakeGarbageCollected<NGExclusion>(rect, type, std::move(shape_data));
  }

  const NGExclusion* CopyWithOffset(const NGBfcDelta& offset_delta) const {
    if (!offset_delta.line_offset_delta && !offset_delta.block_offset_delta)
      return this;

    NGBfcRect new_rect = rect;
    new_rect.start_offset += offset_delta;
    new_rect.end_offset += offset_delta;

    return MakeGarbageCollected<NGExclusion>(
        new_rect, type,
        shape_data ? MakeGarbageCollected<NGExclusionShapeData>(*shape_data)
                   : nullptr);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(shape_data); }

  const NGBfcRect rect;
  const EFloat type;
  bool is_past_other_exclusions = false;
  const Member<const NGExclusionShapeData> shape_data;

  bool operator==(const NGExclusion& other) const;
  bool operator!=(const NGExclusion& other) const { return !(*this == other); }
};

using NGExclusionPtrArray = HeapVector<Member<const NGExclusion>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_EXCLUSION_H_
