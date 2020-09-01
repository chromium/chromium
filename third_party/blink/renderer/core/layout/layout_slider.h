/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SLIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SLIDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class HTMLInputElement;

class CORE_EXPORT LayoutSlider final : public LayoutFlexibleBox {
 public:
  static const int kDefaultTrackLength;

  explicit LayoutSlider(HTMLInputElement*);
  ~LayoutSlider() override;

  const char* GetName() const override { return "LayoutSlider"; }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSlider || LayoutFlexibleBox::IsOfType(type);
  }

  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
};

template <>
struct DowncastTraits<LayoutSlider> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSlider();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SLIDER_H_
