/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
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

#include "third_party/blink/renderer/core/svg/svg_animated_path.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/svg/svg_path_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGAnimatedPath::SVGAnimatedPath(SVGElement* context_element,
                                 const QualifiedName& attribute_name,
                                 CSSPropertyID css_property_id)
    : SVGAnimatedProperty<SVGPath>(context_element,
                                   attribute_name,
                                   MakeGarbageCollected<SVGPath>(),
                                   css_property_id) {}

SVGAnimatedPath::~SVGAnimatedPath() = default;

const CSSValue* SVGAnimatedPath::CssValue() const {
  DCHECK(HasPresentationAttributeMapping());
  const SVGAnimatedPath* path = this;
  // If this is a <use> instance, return the referenced path to maximize
  // geometry sharing.
  if (const SVGElement* element = ContextElement()->CorrespondingElement()) {
    path = To<SVGPathElement>(element)->GetPath();
  }
  const cssvalue::CSSPathValue& path_value = path->CurrentValue()->PathValue();
  if (path_value.GetStylePath()->ByteStream().IsEmpty())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  return &path_value;
}

}  // namespace blink
