/*
 * Copyright (C) 2007-2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_style_declaration.h"

#include <algorithm>

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_bitsets.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or
// lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
bool HasCSSPropertyNamePrefix(const AtomicString& property_name,
                              const char* prefix) {
#if DCHECK_IS_ON()
  DCHECK(*prefix);
  for (const char* p = prefix; *p; ++p) {
    DCHECK(IsASCIILower(*p));
  }
  DCHECK(property_name.length());
#endif

  if (ToASCIILower(property_name[0]) != prefix[0]) {
    return false;
  }

  unsigned length = property_name.length();
  for (unsigned i = 1; i < length; ++i) {
    if (!prefix[i]) {
      return IsASCIIUpper(property_name[i]);
    }
    if (property_name[i] != prefix[i]) {
      return false;
    }
  }
  return false;
}

CSSPropertyID ParseCSSPropertyID(const ExecutionContext* execution_context,
                                 const AtomicString& property_name) {
  unsigned length = property_name.length();
  if (!length) {
    return CSSPropertyID::kInvalid;
  }

  StringBuilder builder;
  builder.ReserveCapacity(length);

  unsigned i = 0;
  bool has_seen_dash = false;

  if (HasCSSPropertyNamePrefix(property_name, "webkit")) {
    builder.Append('-');
  } else if (IsASCIIUpper(property_name[0])) {
    return CSSPropertyID::kInvalid;
  }

  bool has_seen_upper = IsASCIIUpper(property_name[i]);

  builder.Append(ToASCIILower(property_name[i++]));

  for (; i < length; ++i) {
    UChar c = property_name[i];
    if (!IsASCIIUpper(c)) {
      if (c == '-') {
        has_seen_dash = true;
      }
      builder.Append(c);
    } else {
      has_seen_upper = true;
      builder.Append('-');
      builder.Append(ToASCIILower(c));
    }
  }

  // Reject names containing both dashes and upper-case characters, such as
  // "border-rightColor".
  if (has_seen_dash && has_seen_upper) {
    return CSSPropertyID::kInvalid;
  }

  String prop_name = builder.ReleaseString();
  return UnresolvedCSSPropertyID(execution_context, prop_name);
}

// When getting properties on CSSStyleDeclarations, the name used from
// Javascript and the actual name of the property are not the same, so
// we have to do the following translation. The translation turns upper
// case characters into lower case characters and inserts dashes to
// separate words.
//
// Example: 'backgroundPositionY' -> 'background-position-y'
//
// Also, certain prefixes such as 'css-' are stripped.
CSSPropertyID CssPropertyInfo(const ExecutionContext* execution_context,
                              const AtomicString& name) {
  typedef HashMap<String, CSSPropertyID> CSSPropertyIDMap;
  DEFINE_STATIC_LOCAL(CSSPropertyIDMap, map, ());
  CSSPropertyIDMap::iterator iter = map.find(name);
  if (iter != map.end()) {
    return iter->value;
  }

  CSSPropertyID unresolved_property =
      ParseCSSPropertyID(execution_context, name);
  if (unresolved_property == CSSPropertyID::kVariable) {
    unresolved_property = CSSPropertyID::kInvalid;
  }
  // Only cache known-exposed properties (i.e. properties without any
  // associated runtime flag). This is because the web-exposure of properties
  // that are not known-exposed can change dynamically, for example when
  // different ExecutionContexts are provided with different origin trial
  // settings.
  if (kKnownExposedProperties.Has(unresolved_property)) {
    map.insert(name, unresolved_property);
  }
  DCHECK(!IsValidCSSPropertyID(unresolved_property) ||
         CSSProperty::Get(ResolveCSSPropertyID(unresolved_property))
             .IsWebExposed(execution_context));
  return unresolved_property;
}

}  // namespace

void CSSStyleDeclaration::Trace(Visitor* visitor) const {
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

CSSStyleDeclaration::CSSStyleDeclaration(ExecutionContext* context)
    : ExecutionContextClient(context) {}

CSSStyleDeclaration::~CSSStyleDeclaration() = default;

void CSSStyleDeclaration::setCSSFloat(const ExecutionContext* execution_context,
                                      const String& value,
                                      ExceptionState& exception_state) {
  SetPropertyInternal(CSSPropertyID::kFloat, String(), value, false,
                      execution_context->GetSecureContextMode(),
                      exception_state);
}

String CSSStyleDeclaration::AnonymousNamedGetter(const AtomicString& name) {
  // Search the style declaration.
  CSSPropertyID unresolved_property =
      CssPropertyInfo(GetExecutionContext(), name);

  // Do not handle non-property names.
  if (!IsValidCSSPropertyID(unresolved_property)) {
    return String();
  }

  return GetPropertyValueInternal(ResolveCSSPropertyID(unresolved_property));
}

NamedPropertySetterResult CSSStyleDeclaration::AnonymousNamedSetter(
    ScriptState* script_state,
    const AtomicString& name,
    v8::Local<v8::Value> value) {
  const ExecutionContext* execution_context =
      ExecutionContext::From(script_state);
  if (!execution_context) {
    return NamedPropertySetterResult::kDidNotIntercept;
  }
  CSSPropertyID unresolved_property = CssPropertyInfo(execution_context, name);
  if (!IsValidCSSPropertyID(unresolved_property)) {
    return NamedPropertySetterResult::kDidNotIntercept;
  }
  // We create the ExceptionState manually due to performance issues: adding
  // [RaisesException] to the IDL causes the bindings layer to expensively
  // create a std::string to set the ExceptionState's |property_name| argument,
  // while we can use CSSProperty::GetPropertyName() here (see bug 829408).
  ExceptionState exception_state(
      script_state->GetIsolate(), ExceptionState::kSetterContext,
      "CSSStyleDeclaration",
      CSSProperty::Get(ResolveCSSPropertyID(unresolved_property))
          .GetPropertyName());
  if (value->IsNumber()) {
    double double_value = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(
        script_state->GetIsolate(), value, exception_state);
    if (UNLIKELY(exception_state.HadException())) {
      return NamedPropertySetterResult::kIntercepted;
    }
    if (FastPathSetProperty(unresolved_property, double_value)) {
      return NamedPropertySetterResult::kIntercepted;
    }
    // The fast path failed, e.g. because the property was a longhand,
    // so let the normal string handling deal with it.
  }
  // Perform a type conversion from ES value to
  // IDL [LegacyNullToEmptyString] DOMString only after we've confirmed that
  // the property name is a valid CSS attribute name (see bug 1310062).
  auto&& string_value =
      NativeValueTraits<IDLStringTreatNullAsEmptyString>::NativeValue(
          script_state->GetIsolate(), value, exception_state);
  if (UNLIKELY(exception_state.HadException())) {
    return NamedPropertySetterResult::kIntercepted;
  }
  SetPropertyInternal(unresolved_property, String(), string_value, false,
                      execution_context->GetSecureContextMode(),
                      exception_state);
  if (exception_state.HadException()) {
    return NamedPropertySetterResult::kIntercepted;
  }
  return NamedPropertySetterResult::kIntercepted;
}

NamedPropertyDeleterResult CSSStyleDeclaration::AnonymousNamedDeleter(
    const AtomicString& name) {
  // Pretend to be deleted since web author can define their own property with
  // the same name.
  return NamedPropertyDeleterResult::kDeleted;
}

void CSSStyleDeclaration::NamedPropertyEnumerator(Vector<String>& names,
                                                  ExceptionState&) {
  typedef Vector<String, kNumCSSProperties - 1> PreAllocatedPropertyVector;
  DEFINE_STATIC_LOCAL(PreAllocatedPropertyVector, property_names, ());

  const ExecutionContext* execution_context = GetExecutionContext();

  if (property_names.empty()) {
    for (CSSPropertyID property_id : CSSPropertyIDList()) {
      const CSSProperty& property_class =
          CSSProperty::Get(ResolveCSSPropertyID(property_id));
      if (property_class.IsWebExposed(execution_context)) {
        property_names.push_back(property_class.GetJSPropertyName());
      }
    }
    for (CSSPropertyID property_id : kCSSPropertyAliasList) {
      const CSSUnresolvedProperty* property_class =
          CSSUnresolvedProperty::GetAliasProperty(property_id);
      if (property_class->IsWebExposed(execution_context)) {
        property_names.push_back(property_class->GetJSPropertyName());
      }
    }
    std::sort(property_names.begin(), property_names.end(),
              WTF::CodeUnitCompareLessThan);
  }
  names = property_names;
}

bool CSSStyleDeclaration::NamedPropertyQuery(const AtomicString& name,
                                             ExceptionState&) {
  return IsValidCSSPropertyID(CssPropertyInfo(GetExecutionContext(), name));
}

}  // namespace blink
