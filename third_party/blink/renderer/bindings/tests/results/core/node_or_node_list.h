// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_NODE_OR_NODE_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_NODE_OR_NODE_LIST_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Node;
class NodeList;

class CORE_EXPORT NodeOrNodeList final {
  DISALLOW_NEW();
 public:
  NodeOrNodeList();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsNode() const { return type_ == SpecificType::kNode; }
  Node* GetAsNode() const;
  void SetNode(Node*);
  static NodeOrNodeList FromNode(Node*);

  bool IsNodeList() const { return type_ == SpecificType::kNodeList; }
  NodeList* GetAsNodeList() const;
  void SetNodeList(NodeList*);
  static NodeOrNodeList FromNodeList(NodeList*);

  NodeOrNodeList(const NodeOrNodeList&);
  ~NodeOrNodeList();
  NodeOrNodeList& operator=(const NodeOrNodeList&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kNode,
    kNodeList,
  };
  SpecificType type_;

  Member<Node> node_;
  Member<NodeList> node_list_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const NodeOrNodeList&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8NodeOrNodeList final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, NodeOrNodeList&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const NodeOrNodeList&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, NodeOrNodeList& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, NodeOrNodeList& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<NodeOrNodeList> : public NativeValueTraitsBase<NodeOrNodeList> {
  CORE_EXPORT static NodeOrNodeList NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static NodeOrNodeList NullValue() { return NodeOrNodeList(); }
};

template <>
struct V8TypeOf<NodeOrNodeList> {
  typedef V8NodeOrNodeList Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::NodeOrNodeList);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_NODE_OR_NODE_LIST_H_
