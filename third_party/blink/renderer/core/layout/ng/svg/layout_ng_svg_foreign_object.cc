// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_foreign_object.h"

#include "third_party/blink/renderer/core/svg_element_type_helpers.h"

namespace blink {

LayoutNGSVGForeignObject::LayoutNGSVGForeignObject(Element* element)
    : LayoutNGBlockFlowMixin<LayoutSVGBlock>(element) {
  DCHECK(IsA<SVGForeignObjectElement>(element));
}

const char* LayoutNGSVGForeignObject::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGSVGForeignObject";
}

bool LayoutNGSVGForeignObject::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGSVGForeignObject ||
         LayoutNGBlockFlowMixin<LayoutSVGBlock>::IsOfType(type);
}

}  // namespace blink
