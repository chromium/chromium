/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"

#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

const CSSValue* ComputedStyleCSSValueMapping::Get(
    const AtomicString& custom_property_name,
    const ComputedStyle& style,
    const PropertyRegistry* registry,
    CSSValuePhase value_phase) {
  CustomProperty custom_property(custom_property_name, registry);
  return custom_property.CSSValueFromComputedStyle(
      style, nullptr /* layout_object */, false /* allow_visited_style */,
      value_phase);
}

HeapHashMap<AtomicString, Member<const CSSValue>>
ComputedStyleCSSValueMapping::GetVariables(const ComputedStyle& style,
                                           const PropertyRegistry* registry,
                                           CSSValuePhase value_phase) {
  HeapHashMap<AtomicString, Member<const CSSValue>> variables;

  for (const AtomicString& name : style.GetVariableNames()) {
    const CSSValue* value =
        ComputedStyleCSSValueMapping::Get(name, style, registry, value_phase);
    if (value) {
      variables.Set(name, value);
    }
  }

  return variables;
}

}  // namespace blink
