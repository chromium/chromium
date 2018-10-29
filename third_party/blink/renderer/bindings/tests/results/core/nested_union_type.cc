// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/nested_union_type.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/byte_string_or_node_list.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_xml_http_request.h"
#include "third_party/blink/renderer/core/dom/name_node_list.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"

namespace blink {

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord() : type_(SpecificType::kNone) {}

Event* NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::GetAsEvent() const {
  DCHECK(IsEvent());
  return event_;
}

void NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SetEvent(Event* value) {
  DCHECK(IsNull());
  event_ = value;
  type_ = SpecificType::kEvent;
}

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::FromEvent(Event* value) {
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord container;
  container.SetEvent(value);
  return container;
}

const Vector<int32_t>& NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::GetAsLongSequence() const {
  DCHECK(IsLongSequence());
  return long_sequence_;
}

void NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SetLongSequence(const Vector<int32_t>& value) {
  DCHECK(IsNull());
  long_sequence_ = value;
  type_ = SpecificType::kLongSequence;
}

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::FromLongSequence(const Vector<int32_t>& value) {
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord container;
  container.SetLongSequence(value);
  return container;
}

Node* NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::GetAsNode() const {
  DCHECK(IsNode());
  return node_;
}

void NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SetNode(Node* value) {
  DCHECK(IsNull());
  node_ = value;
  type_ = SpecificType::kNode;
}

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::FromNode(Node* value) {
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord container;
  container.SetNode(value);
  return container;
}

const String& NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::GetAsString() const {
  DCHECK(IsString());
  return string_;
}

void NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SetString(const String& value) {
  DCHECK(IsNull());
  string_ = value;
  type_ = SpecificType::kString;
}

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::FromString(const String& value) {
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord container;
  container.SetString(value);
  return container;
}

const HeapVector<std::pair<String, ByteStringOrNodeList>>& NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::GetAsStringByteStringOrNodeListRecord() const {
  DCHECK(IsStringByteStringOrNodeListRecord());
  return string_byte_string_or_node_list_record_;
}

void NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SetStringByteStringOrNodeListRecord(const HeapVector<std::pair<String, ByteStringOrNodeList>>& value) {
  DCHECK(IsNull());
  string_byte_string_or_node_list_record_ = value;
  type_ = SpecificType::kStringByteStringOrNodeListRecord;
}

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::FromStringByteStringOrNodeListRecord(const HeapVector<std::pair<String, ByteStringOrNodeList>>& value) {
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord container;
  container.SetStringByteStringOrNodeListRecord(value);
  return container;
}

XMLHttpRequest* NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::GetAsXMLHttpRequest() const {
  DCHECK(IsXMLHttpRequest());
  return xml_http_request_;
}

void NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SetXMLHttpRequest(XMLHttpRequest* value) {
  DCHECK(IsNull());
  xml_http_request_ = value;
  type_ = SpecificType::kXMLHttpRequest;
}

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::FromXMLHttpRequest(XMLHttpRequest* value) {
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord container;
  container.SetXMLHttpRequest(value);
  return container;
}

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&) = default;
NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::~NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord() = default;
NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::operator=(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&) = default;

void NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::Trace(blink::Visitor* visitor) {
  visitor->Trace(event_);
  visitor->Trace(node_);
  visitor->Trace(string_byte_string_or_node_list_record_);
  visitor->Trace(xml_http_request_);
}

void V8NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& impl, UnionTypeConversionMode conversionMode, ExceptionState& exceptionState) {
  if (v8Value.IsEmpty())
    return;

  if (conversionMode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8Value))
    return;

  if (V8Event::hasInstance(v8Value, isolate)) {
    Event* cppValue = V8Event::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetEvent(cppValue);
    return;
  }

  if (V8Node::hasInstance(v8Value, isolate)) {
    Node* cppValue = V8Node::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetNode(cppValue);
    return;
  }

  if (V8XMLHttpRequest::hasInstance(v8Value, isolate)) {
    XMLHttpRequest* cppValue = V8XMLHttpRequest::ToImpl(v8::Local<v8::Object>::Cast(v8Value));
    impl.SetXMLHttpRequest(cppValue);
    return;
  }

  if (HasCallableIteratorSymbol(isolate, v8Value, exceptionState)) {
    Vector<int32_t> cppValue = NativeValueTraits<IDLSequence<IDLLong>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetLongSequence(cppValue);
    return;
  }

  if (v8Value->IsObject()) {
    HeapVector<std::pair<String, ByteStringOrNodeList>> cppValue = NativeValueTraits<IDLRecord<IDLString, ByteStringOrNodeList>>::NativeValue(isolate, v8Value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.SetStringByteStringOrNodeListRecord(cppValue);
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

v8::Local<v8::Value> ToV8(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SpecificType::kNone:
      return v8::Null(isolate);
    case NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SpecificType::kEvent:
      return ToV8(impl.GetAsEvent(), creationContext, isolate);
    case NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SpecificType::kLongSequence:
      return ToV8(impl.GetAsLongSequence(), creationContext, isolate);
    case NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SpecificType::kNode:
      return ToV8(impl.GetAsNode(), creationContext, isolate);
    case NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SpecificType::kString:
      return V8String(isolate, impl.GetAsString());
    case NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SpecificType::kStringByteStringOrNodeListRecord:
      return ToV8(impl.GetAsStringByteStringOrNodeListRecord(), creationContext, isolate);
    case NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::SpecificType::kXMLHttpRequest:
      return ToV8(impl.GetAsXMLHttpRequest(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NativeValueTraits<NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord impl;
  V8NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exceptionState);
  return impl;
}

}  // namespace blink
