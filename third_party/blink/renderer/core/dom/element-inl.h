// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_INL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_INL_H_

#include "third_party/blink/renderer/core/dom/element.h"

#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

inline ElementRareData* Element::GetElementRareData() const {
  DCHECK(HasRareData());
  return static_cast<ElementRareData*>(RareData());
}

inline ElementRareData& Element::EnsureElementRareData() {
  return static_cast<ElementRareData&>(EnsureRareData());
}

inline void Element::SynchronizeAttribute(const QualifiedName& name) const {
  if (!GetElementData())
    return;
  if (UNLIKELY(name == html_names::kStyleAttr &&
               GetElementData()->style_attribute_is_dirty())) {
    DCHECK(IsStyledElement());
    SynchronizeStyleAttributeInternal();
    return;
  }
  if (UNLIKELY(GetElementData()->svg_attributes_are_dirty())) {
    // See comment in the AtomicString version of SynchronizeAttribute()
    // also.
    To<SVGElement>(this)->SynchronizeSVGAttribute(name);
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_INL_H_
