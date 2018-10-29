// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/xml_http_request_or_string.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_xml_http_request.h"

namespace blink {

XMLHttpRequestOrString::XMLHttpRequestOrString() : type_(SpecificType::kNone) {}

const String& XMLHttpRequestOrString::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void XMLHttpRequestOrString::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

XMLHttpRequestOrString XMLHttpRequestOrString::FromString(const String& value) {
  XMLHttpRequestOrString container;
  container.SetString(value);
  return container;
}

XMLHttpRequest* XMLHttpRequestOrString::GetAsXMLHttpRequest() const {
  DCHECK(IsXMLHttpRequest());
  return xml_http_request_;
}

void XMLHttpRequestOrString::SetXMLHttpRequest(XMLHttpRequest* value) {
  DCHECK(IsNull());
  xml_http_request_ = value;
  type_ = SpecificType::kXMLHttpRequest;
}

XMLHttpRequestOrString XMLHttpRequestOrString::FromXMLHttpRequest(XMLHttpRequest* value) {
  XMLHttpRequestOrString container;
  container.SetXMLHttpRequest(value);
  return container;
}

XMLHttpRequestOrString::XMLHttpRequestOrString(const XMLHttpRequestOrString&) = default;
XMLHttpRequestOrString::~XMLHttpRequestOrString() = default;
XMLHttpRequestOrString& XMLHttpRequestOrString::operator=(const XMLHttpRequestOrString&) = default;

void XMLHttpRequestOrString::Trace(blink::Visitor* visitor) {
  visitor->Trace(xml_http_request_);
}

void V8XMLHttpRequestOrString::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, XMLHttpRequestOrString& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (V8XMLHttpRequest::hasInstance(v8Value, isolate)) {
    XMLHttpRequest* cppValue = V8XMLHttpRequest::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetXMLHttpRequest(cppValue);
    return;
  }

  {
    V8StringResource<> cppValue = v8Value;
    if (!cppValue.Prepare(exceptionState))
      return;
    impl.SetString(cppValue);
    return;
  }
}

v8::Local<v8::Value> ToV8(const XMLHttpRequestOrString& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case XMLHttpRequestOrString::SpecificType::kNone:
      return v8::Null(isolate);
    case XMLHttpRequestOrString::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    case XMLHttpRequestOrString::SpecificType::kXMLHttpRequest:
      return ToV8(impl.GetAsXMLHttpRequest(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

XMLHttpRequestOrString NativeValueTraits<XMLHttpRequestOrString>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  XMLHttpRequestOrString impl;
  V8XMLHttpRequestOrString::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
