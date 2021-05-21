// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/node_or_node_list.h"

#include "base/cxx17_backports.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_list.h"
#include "third_party/blink/renderer/core/dom/name_node_list.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"

namespace blink {

NodeOrNodeList::NodeOrNodeList() : type_(SpecificType::kNone) {}

Node* NodeOrNodeList::GetAsNode() const {
  DCHECK(IsNode());
  return node_;
}

void NodeOrNodeList::SetNode(Node* value) {
  DCHECK(IsNull());
  node_ = value;
  type_ = SpecificType::kNode;
}

NodeOrNodeList NodeOrNodeList::FromNode(Node* value) {
  NodeOrNodeList container;
  container.SetNode(value);
  return container;
}

NodeList* NodeOrNodeList::GetAsNodeList() const {
  DCHECK(IsNodeList());
  return node_list_;
}

void NodeOrNodeList::SetNodeList(NodeList* value) {
  DCHECK(IsNull());
  node_list_ = value;
  type_ = SpecificType::kNodeList;
}

NodeOrNodeList NodeOrNodeList::FromNodeList(NodeList* value) {
  NodeOrNodeList container;
  container.SetNodeList(value);
  return container;
}

NodeOrNodeList::NodeOrNodeList(const NodeOrNodeList&) = default;
NodeOrNodeList::~NodeOrNodeList() = default;
NodeOrNodeList& NodeOrNodeList::operator=(const NodeOrNodeList&) = default;

void NodeOrNodeList::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(node_list_);
}

void V8NodeOrNodeList::ToImpl(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_value,
    NodeOrNodeList& impl,
    UnionTypeConversionMode conversion_mode,
    ExceptionState& exception_state) {
  if (v8_value.IsEmpty())
    return;

  if (conversion_mode == UnionTypeConversionMode::kNullable && IsUndefinedOrNull(v8_value))
    return;

  if (V8Node::HasInstance(v8_value, isolate)) {
    Node* cpp_value = V8Node::ToImpl(v8::Local<v8::Object>::Cast(v8_value));
    impl.SetNode(cpp_value);
    return;
  }

  if (V8NodeList::HasInstance(v8_value, isolate)) {
    NodeList* cpp_value = V8NodeList::ToImpl(v8::Local<v8::Object>::Cast(v8_value));
    impl.SetNodeList(cpp_value);
    return;
  }

  exception_state.ThrowTypeError("The provided value is not of type '(Node or NodeList)'");
}

v8::Local<v8::Value> ToV8(const NodeOrNodeList& impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  switch (impl.type_) {
    case NodeOrNodeList::SpecificType::kNone:
      return v8::Null(isolate);
    case NodeOrNodeList::SpecificType::kNode:
      return ToV8(impl.GetAsNode(), creationContext, isolate);
    case NodeOrNodeList::SpecificType::kNodeList:
      return ToV8(impl.GetAsNodeList(), creationContext, isolate);
    default:
      NOTREACHED();
  }
  return v8::Local<v8::Value>();
}

NodeOrNodeList NativeValueTraits<NodeOrNodeList>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  NodeOrNodeList impl;
  V8NodeOrNodeList::ToImpl(isolate, value, impl, UnionTypeConversionMode::kNotNullable, exception_state);
  return impl;
}

}  // namespace blink

