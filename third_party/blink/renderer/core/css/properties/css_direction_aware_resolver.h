// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_DIRECTION_AWARE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_DIRECTION_AWARE_RESOLVER_H_

#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CSSProperty;
class StylePropertyShorthand;

class CSSDirectionAwareResolver {
  STATIC_ONLY(CSSDirectionAwareResolver);

 public:
  // A group of physical properties that's used by the 'Resolve*' functions
  // to convert a direction-aware property into a physical property.
  template <size_t size>
  class PhysicalGroup {
   public:
    PhysicalGroup(const StylePropertyShorthand&);
    PhysicalGroup(const CSSProperty* (&properties)[size]);
    const CSSProperty& GetProperty(size_t index) const;

   private:
    const CSSProperty** properties_;
  };

  static PhysicalGroup<4> BorderGroup();
  static PhysicalGroup<4> BorderColorGroup();
  static PhysicalGroup<4> BorderStyleGroup();
  static PhysicalGroup<4> BorderWidthGroup();
  static PhysicalGroup<4> InsetGroup();
  static PhysicalGroup<2> IntrinsicSizeGroup();
  static PhysicalGroup<4> MarginGroup();
  static PhysicalGroup<2> MaxSizeGroup();
  static PhysicalGroup<2> MinSizeGroup();
  static PhysicalGroup<2> OverflowGroup();
  static PhysicalGroup<2> OverscrollBehaviorGroup();
  static PhysicalGroup<4> PaddingGroup();
  static PhysicalGroup<4> ScrollMarginGroup();
  static PhysicalGroup<4> ScrollPaddingGroup();
  static PhysicalGroup<2> SizeGroup();
  static PhysicalGroup<4> VisitedBorderColorGroup();

  // These resolvers expect a PhysicalGroup with box sides, in the following
  // order: top, right, bottom, left.
  static const CSSProperty& ResolveInlineStart(TextDirection,
                                               WritingMode,
                                               const PhysicalGroup<4>&);
  static const CSSProperty& ResolveInlineEnd(TextDirection,
                                             WritingMode,
                                             const PhysicalGroup<4>&);
  static const CSSProperty& ResolveBlockStart(TextDirection,
                                              WritingMode,
                                              const PhysicalGroup<4>&);
  static const CSSProperty& ResolveBlockEnd(TextDirection,
                                            WritingMode,
                                            const PhysicalGroup<4>&);

  // These resolvers expect a PhysicalGroup with dimensions, in the following
  // order: horizontal, vertical.
  static const CSSProperty& ResolveInline(TextDirection,
                                          WritingMode,
                                          const PhysicalGroup<2>&);
  static const CSSProperty& ResolveBlock(TextDirection,
                                         WritingMode,
                                         const PhysicalGroup<2>&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_DIRECTION_AWARE_RESOLVER_H_
