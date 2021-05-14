// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_animated_string.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_trustedscripturl.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

namespace blink {

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

V8UnionStringOrTrustedScriptURL* SVGAnimatedString::baseVal() {
  return MakeGarbageCollected<V8UnionStringOrTrustedScriptURL>(
      SVGAnimatedProperty<SVGString>::baseVal());
}

void SVGAnimatedString::setBaseVal(const V8UnionStringOrTrustedScriptURL* value,
                                   ExceptionState& exception_state) {
  DCHECK(value);

  // https://w3c.github.io/webappsec-trusted-types/dist/spec/#integration-with-svg
  String string;
  switch (value->GetContentType()) {
    case V8UnionStringOrTrustedScriptURL::ContentType::kString:
      string = value->GetAsString();
      if (ContextElement()->IsScriptElement()) {
        string = TrustedTypesCheckForScriptURL(
            string, ContextElement()->GetExecutionContext(), exception_state);
        if (exception_state.HadException())
          return;
      }
      break;
    case V8UnionStringOrTrustedScriptURL::ContentType::kTrustedScriptURL:
      string = value->GetAsTrustedScriptURL()->toString();
      break;
  }
  SVGAnimatedProperty<SVGString>::setBaseVal(string, exception_state);
}

#else  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

void SVGAnimatedString::setBaseVal(
    const StringOrTrustedScriptURL& string_or_trusted_script_url,
    ExceptionState& exception_state) {
  String value;
  // See:
  // https://w3c.github.io/webappsec-trusted-types/dist/spec/#integration-with-svg
  if (string_or_trusted_script_url.IsTrustedScriptURL()) {
    value = string_or_trusted_script_url.GetAsTrustedScriptURL()->toString();
  } else {
    value = string_or_trusted_script_url.GetAsString();
    if (ContextElement()->IsScriptElement()) {
      value = TrustedTypesCheckForScriptURL(
          value, ContextElement()->GetExecutionContext(), exception_state);
      if (exception_state.HadException())
        return;
    }
  }
  SVGAnimatedProperty<SVGString>::setBaseVal(value, exception_state);
}

void SVGAnimatedString::baseVal(
    StringOrTrustedScriptURL& string_or_trusted_script_url) {
  string_or_trusted_script_url.SetString(
      SVGAnimatedProperty<SVGString>::baseVal());
}

#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

String SVGAnimatedString::animVal() {
  return SVGAnimatedProperty<SVGString>::animVal();
}

void SVGAnimatedString::Trace(Visitor* visitor) const {
  SVGAnimatedProperty<SVGString>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
