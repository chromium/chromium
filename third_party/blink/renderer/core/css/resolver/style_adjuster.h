/*
 * Copyright (C) 2013 Google, Inc.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_ADJUSTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_ADJUSTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class ComputedStyle;
class ComputedStyleBuilder;
class StyleResolverState;
class SVGElement;

// Certain CSS Properties/Values do not apply to certain elements
// and the web expects that we expose "adjusted" values when
// for those property/element pairs.
class StyleAdjuster {
  STATIC_ONLY(StyleAdjuster);

 public:
  CORE_EXPORT static void AdjustComputedStyle(StyleResolverState&, Element*);
  static void AdjustStyleForCombinedText(ComputedStyleBuilder&);
  static void AdjustStyleForTextCombine(ComputedStyleBuilder&);
  static void AdjustStyleForEditing(ComputedStyleBuilder&, Element*);
  static void AdjustStyleForDisplay(ComputedStyleBuilder&,
                                    const ComputedStyle& layout_parent_style,
                                    const Element*,
                                    Document*);

  // A general note on caching: The StyleAdjuster's results can be cached
  // in the MatchedPropertiesCache (MPC). However, the StyleAdjuster
  // frequently draws in information from many other things than the set
  // of matched properties (which is normally the MPC's key), which we
  // solve by two mechanisms:
  //
  // First, all MPC entries are keyed by element type, as e.g.
  // HTMLBRElement has special handling of display:contents that e.g.
  // HTMLDivElement does not. We support a certain amount of grouping,
  // though, since e.g. <div> and <span> behave the same for the purposes
  // of the StyleAdjuster and we don't really need to distinguish between
  // them. (We also do some post-hoc checking of MPC hits to verify that
  // e.g. layout parent style is the same between the cache entry and the
  // element we are trying to compute style for.) See
  // GetElementTypeCacheKey().
  //
  // Second, the StyleAdjuster is allowed to disable caching, typically
  // for situations where it would not be deterministic for the same
  // ComputedStyleBuilder state (i.e., applied and inherited properties)
  // and element type. This is governed by CanCacheStyleAdjustment()
  // returning kIsNotElement (used as sentinel value for “uncachable”).
  //
  // A typical example where you would return false is if StyleAdjustment
  // is different depending on an attribute on the element;
  // HTML attributes are not part of the MPC cache key. But if you want
  // (and it doesn't take too much time), you can check that the attribute
  // is either one value or the other, and return true for one of them
  // (ideally the most common one) instead of false for both; it only
  // needs to be deterministic for the cases where you return true.

  // Used to know what type of element was given to the StyleAdjuster.
  // kIsNotElement is used as a sentinel value to mark that StyleAdjuster
  // was not run on the cached entry.
  struct ElementTypeForCache {
    ElementType element_type;

    bool CacheEntryIsStyleAdjusted() const {
      return element_type != ElementType::kIsNotElement;
    }

    bool operator==(const ElementTypeForCache& other) const {
      return element_type == other.element_type;
    }
  };
  static ElementTypeForCache GetElementTypeCacheKey(
      const ComputedStyle& layout_parent_style,
      const Element& element);

  // Run the part of style adjustment that cannot be cached in the MPC,
  // after the normal AdjustComputedStyle() call. This typically encompasses
  // things that are not worth putting in GetElementTypeCacheKey(),
  // because the actual adjustment is so cheap compared to the test
  // (so taking those items out of the cache would be as expensive
  // as just doing the adjustment).
  static void RunUncacheableStyleAdjustment(
      ComputedStyleBuilder& builder,
      Element& element,
      const Element* element_or_pseudo_element,
      const Element* styled_element);

  // Whether a cache entry for A would have the same style adjustments as
  // for B, or vice versa.
  static bool IsCacheCompatible(const ComputedStyle& parent_style_a,
                                const ComputedStyle& layout_parent_style_a,
                                const ComputedStyle& parent_style_b,
                                const ComputedStyle& layout_parent_style_b);

 private:
  static void AdjustStyleForSvgElement(
      const SVGElement& element,
      const SVGElement* styled_element,
      ComputedStyleBuilder& builder,
      const ComputedStyle& layout_parent_style);
  static void AdjustStyleForHTMLElement(ComputedStyleBuilder&, HTMLElement&);
  static void AdjustSliderContainerStyle(const Element& element,
                                         ComputedStyleBuilder& builder);

  static bool IsEditableElement(Element*, const ComputedStyleBuilder&);
  static bool IsPasswordFieldWithUnrevealedPassword(Element*);
  static void AdjustEffectiveTouchAction(ComputedStyleBuilder&,
                                         const ComputedStyle& parent_style,
                                         Element* element,
                                         bool is_svg_root);
  static void AdjustOverflow(ComputedStyleBuilder&,
                             Element* element,
                             Document&);
  static void AdjustForForcedColorsMode(ComputedStyleBuilder&, Document&);
  static void AdjustForSVGTextElement(ComputedStyleBuilder&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_ADJUSTER_H_
