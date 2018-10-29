// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_NESTED_UNION_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_NESTED_UNION_TYPE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ByteStringOrNodeList;
class Event;
class Node;
class XMLHttpRequest;

class CORE_EXPORT NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord final {
  DISALLOW_NEW();
 public:
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsEvent() const { return type_ == SpecificType::kEvent; }
  Event* GetAsEvent() const;
  void SetEvent(Event*);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord FromEvent(Event*);

  bool IsLongSequence() const { return type_ == SpecificType::kLongSequence; }
  const Vector<int32_t>& GetAsLongSequence() const;
  void SetLongSequence(const Vector<int32_t>&);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord FromLongSequence(const Vector<int32_t>&);

  bool IsNode() const { return type_ == SpecificType::kNode; }
  Node* GetAsNode() const;
  void SetNode(Node*);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord FromNode(Node*);

  bool IsString() const { return type_ == SpecificType::kString; }
  const String& GetAsString() const;
  void SetString(const String&);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord FromString(const String&);

  bool IsStringByteStringOrNodeListRecord() const { return type_ == SpecificType::kStringByteStringOrNodeListRecord; }
  const HeapVector<std::pair<String, ByteStringOrNodeList>>& GetAsStringByteStringOrNodeListRecord() const;
  void SetStringByteStringOrNodeListRecord(const HeapVector<std::pair<String, ByteStringOrNodeList>>&);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord FromStringByteStringOrNodeListRecord(const HeapVector<std::pair<String, ByteStringOrNodeList>>&);

  bool IsXMLHttpRequest() const { return type_ == SpecificType::kXMLHttpRequest; }
  XMLHttpRequest* GetAsXMLHttpRequest() const;
  void SetXMLHttpRequest(XMLHttpRequest*);
  static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord FromXMLHttpRequest(XMLHttpRequest*);

  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&);
  ~NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord();
  NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& operator=(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kEvent,
    kLongSequence,
    kNode,
    kString,
    kStringByteStringOrNodeListRecord,
    kXMLHttpRequest,
  };
  SpecificType type_;

  Member<Event> event_;
  Vector<int32_t> long_sequence_;
  Member<Node> node_;
  String string_;
  HeapVector<std::pair<String, ByteStringOrNodeList>> string_byte_string_or_node_list_record_;
  Member<XMLHttpRequest> xml_http_request_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord> : public NativeValueTraitsBase<NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord> {
  CORE_EXPORT static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord NullValue() { return NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord(); }
};

template <>
struct V8TypeOf<NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord> {
  typedef V8NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::NodeOrLongSequenceOrEventOrXMLHttpRequestOrStringOrStringByteStringOrNodeListRecord);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_NESTED_UNION_TYPE_H_
