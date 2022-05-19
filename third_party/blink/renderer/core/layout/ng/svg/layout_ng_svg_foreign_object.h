// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_FOREIGN_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_FOREIGN_OBJECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_block.h"

namespace blink {

extern template class CORE_EXTERN_TEMPLATE_EXPORT LayoutNGMixin<LayoutSVGBlock>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    LayoutNGBlockFlowMixin<LayoutSVGBlock>;

// The LayoutNG representation of SVG <foreignObject>.
class LayoutNGSVGForeignObject final
    : public LayoutNGBlockFlowMixin<LayoutSVGBlock> {
 public:
  explicit LayoutNGSVGForeignObject(Element* element);

 private:
  // LayoutObject override:
  const char* GetName() const override;
  bool IsOfType(LayoutObjectType type) const override;
};

template <>
struct DowncastTraits<LayoutNGSVGForeignObject> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsNGSVGForeignObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_SVG_LAYOUT_NG_SVG_FOREIGN_OBJECT_H_
