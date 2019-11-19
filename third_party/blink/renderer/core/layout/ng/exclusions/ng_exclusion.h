// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_EXCLUSION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_EXCLUSION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_rect.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class LayoutBox;

struct CORE_EXPORT NGExclusionShapeData {
  NGExclusionShapeData(const LayoutBox* layout_box,
                       const NGBoxStrut& margins,
                       const NGBoxStrut& shape_insets)
      : layout_box(layout_box), margins(margins), shape_insets(shape_insets) {}

  const LayoutBox* layout_box;
  const NGBoxStrut margins;
  const NGBoxStrut shape_insets;
};

// Struct that represents an exclusion. This currently is just a float but
// we've named it an exclusion to potentially support other types in the future.
struct CORE_EXPORT NGExclusion : public RefCounted<NGExclusion> {
  static scoped_refptr<const NGExclusion> Create(
      const NGBfcRect& rect,
      const EFloat type,
      std::unique_ptr<NGExclusionShapeData> shape_data = nullptr) {
    return base::AdoptRef(new NGExclusion(rect, type, std::move(shape_data)));
  }

  scoped_refptr<const NGExclusion> CopyWithOffset(
      const NGBfcDelta& offset_delta) const {
    if (!offset_delta.line_offset_delta && !offset_delta.block_offset_delta)
      return this;

    NGBfcRect new_rect = rect;
    new_rect.start_offset += offset_delta;
    new_rect.end_offset += offset_delta;

    return base::AdoptRef(new NGExclusion(
        new_rect, type,
        shape_data ? std::make_unique<NGExclusionShapeData>(
                         shape_data->layout_box, shape_data->margins,
                         shape_data->shape_insets)
                   : nullptr));
  }

  const NGBfcRect rect;
  const EFloat type;
  const std::unique_ptr<NGExclusionShapeData> shape_data;

  bool operator==(const NGExclusion& other) const;
  bool operator!=(const NGExclusion& other) const { return !(*this == other); }

 private:
  NGExclusion(const NGBfcRect& rect,
              const EFloat type,
              std::unique_ptr<NGExclusionShapeData> shape_data)
      : rect(rect), type(type), shape_data(std::move(shape_data)) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_EXCLUSION_H_
