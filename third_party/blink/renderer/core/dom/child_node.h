// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_H_

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ChildNode {
  STATIC_ONLY(ChildNode);

 public:
  static void before(
      Node& node,
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      const HeapVector<NodeOrStringOrTrustedScript>& nodes,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      ExceptionState& exception_state) {
    return node.Before(nodes, exception_state);
  }

  static void after(
      Node& node,
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      const HeapVector<NodeOrStringOrTrustedScript>& nodes,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      ExceptionState& exception_state) {
    return node.After(nodes, exception_state);
  }

  static void replaceWith(
      Node& node,
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      const HeapVector<NodeOrStringOrTrustedScript>& nodes,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
      ExceptionState& exception_state) {
    return node.ReplaceWith(nodes, exception_state);
  }

  static void remove(Node& node, ExceptionState& exception_state) {
    return node.remove(exception_state);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_H_
