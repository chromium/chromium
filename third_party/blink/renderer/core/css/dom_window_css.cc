/*
 * Copyright (C) 2012 Motorola Mobility Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Motorola Mobility Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "third_party/blink/renderer/core/css/dom_window_css.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

bool DOMWindowCSS::supports(const ExecutionContext* execution_context,
                            const String& property,
                            const String& value) {
  CSSPropertyID unresolved_property = unresolvedCSSPropertyID(property);
  if (unresolved_property == CSSPropertyID::kInvalid)
    return false;
  if (unresolved_property == CSSPropertyID::kVariable) {
    auto* dummy_style =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
    bool is_animation_tainted = false;
    return CSSParser::ParseValueForCustomProperty(
               dummy_style, "--valid", value, false,
               execution_context->GetSecureContextMode(), nullptr,
               is_animation_tainted)
        .did_parse;
  }

#if DCHECK_IS_ON()
  DCHECK(CSSProperty::Get(resolveCSSPropertyID(unresolved_property))
             .IsWebExposed());
#endif

  // This will return false when !important is present
  auto* dummy_style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  return CSSParser::ParseValue(dummy_style, unresolved_property, value, false,
                               execution_context->GetSecureContextMode())
      .did_parse;
}

bool DOMWindowCSS::supports(const ExecutionContext* execution_context,
                            const String& condition_text) {
  return CSSParser::ParseSupportsCondition(
      condition_text, execution_context->GetSecureContextMode());
}

String DOMWindowCSS::escape(const String& ident) {
  StringBuilder builder;
  SerializeIdentifier(ident, builder);
  return builder.ToString();
}

}  // namespace blink
