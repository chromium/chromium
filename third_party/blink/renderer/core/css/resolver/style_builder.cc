/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands/variable.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void StyleBuilder::ApplyProperty(const CSSPropertyName& name,
                                 StyleResolverState& state,
                                 const CSSValue& value,
                                 ValueMode value_mode) {
  CSSPropertyRef ref(name, state.GetDocument());
  DCHECK(ref.IsValid());

  ApplyProperty(ref.GetProperty(), state, value, value_mode);
}

void StyleBuilder::ApplyProperty(const CSSProperty& property,
                                 StyleResolverState& state,
                                 const CSSValue& value,
                                 ValueMode value_mode) {
  const CSSProperty* physical = &property;
  if (property.IsSurrogate()) {
    physical =
        property.SurrogateFor(state.StyleBuilder().GetWritingDirection());
    DCHECK(physical);
  }
  ApplyPhysicalProperty(*physical, state, value, value_mode);
}

void StyleBuilder::ApplyPhysicalProperty(const CSSProperty& property,
                                         StyleResolverState& state,
                                         const CSSValue& value,
                                         ValueMode value_mode) {
  DCHECK(!Variable::IsStaticInstance(property))
      << "Please use a CustomProperty instance to apply custom properties";
  DCHECK(!property.IsSurrogate())
      << "Please use ApplyProperty for surrogate properties";

  CSSPropertyID id = property.PropertyID();

  // These values must be resolved by StyleCascade before application:
  DCHECK(!value.IsPendingSubstitutionValue());
  DCHECK(!value.IsRevertValue());
  DCHECK(!value.IsRevertLayerValue());
  // CSSUnparsedDeclarationValues should have been resolved as well,
  // *except* for custom properties, which either don't resolve this
  // at all and leaves it unparsed (most cases), or resolves it
  // during CustomProperty::ApplyValue() (registered custom properties
  // with non-universal syntax).
  DCHECK(!value.IsUnparsedDeclaration() || IsA<CustomProperty>(property));

  DCHECK(!property.IsShorthand())
      << "Shorthand property id = " << static_cast<int>(id)
      << " wasn't expanded at parsing time";

  bool is_inherit = value.IsInheritedValue();
  bool is_initial = value.IsInitialValue();
  if (is_inherit && !state.ParentStyle()) {
    is_inherit = false;
    is_initial = true;
  }
  DCHECK(!is_inherit || !is_initial);

  bool is_inherited_for_unset = state.IsInheritedForUnset(property);
  if (is_inherit && !is_inherited_for_unset) {
    state.StyleBuilder().SetHasExplicitInheritance();
    state.ParentStyle()->SetChildHasExplicitInheritance();
  } else if (value.IsUnsetValue()) {
    DCHECK(!is_inherit && !is_initial);
    if (is_inherited_for_unset) {
      is_inherit = true;
    } else {
      is_initial = true;
    }
  }

  if (is_initial) {
    To<Longhand>(property).ApplyInitial(state);
  } else if (is_inherit) {
    To<Longhand>(property).ApplyInherit(state);
  } else {
    To<Longhand>(property).ApplyValue(state, value, value_mode);
  }
}

}  // namespace blink
