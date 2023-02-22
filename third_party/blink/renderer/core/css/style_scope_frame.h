// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_FRAME_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ContainerNode;
class Element;
class StyleScope;
class SelectorChecker;

// The *activations* for a given StyleScope/node, is a list of active
// scopes found in the ancestor chain, their roots (ContainerNode*), and the
// proximities to those roots.
//
// The idea is that, if we're matching a selector ':scope' within some
// StyleScope, we look up the activations for that StyleScope, and
// and check if the current element (`SelectorCheckingContext.element`)
// matches any of the activation roots.
struct CORE_EXPORT StyleScopeActivation {
  DISALLOW_NEW();

 public:
  void Trace(blink::Visitor*) const;

  // The root is the node when the activation happened. In other words,
  // the node that matched <scope-start>. The root is always an Element for
  // activations produced by @scope, however, it may be a non-element for
  // the "default activation" (see SelectorChecker::EnsureActivations).
  //
  // https://drafts.csswg.org/css-cascade-6/#typedef-scope-start
  Member<const ContainerNode> root;
  // The distance to the root, in terms of number of inclusive ancestors
  // between some subject element and the root.
  unsigned proximity = 0;
};

using StyleScopeActivations = HeapVector<StyleScopeActivation>;

// Stores the current @scope activations for a given subject element.
//
// See `StyleScopeActivation` for more information about activations.
//
// StyleScopeFrames are placed on the stack in `Element::RecalcStyle`, and
// serve as a cache of all @scope activations until that point in the tree.
// The actual contents of a StyleScopeFrame is populated lazily during
// `SelectorChecker::CheckPseudoScope`.
//
// StyleScopeFrames may contain a pointer to a parent frame, in which case
// `SelectorChecker::CheckPseudoScope` will store data applicable to the parent
// element in that frame.
class CORE_EXPORT StyleScopeFrame {
  STACK_ALLOCATED();

 public:
  explicit StyleScopeFrame(Element& element) : element_(element) {}

  explicit StyleScopeFrame(Element& element, StyleScopeFrame* parent)
      : element_(element), parent_(parent) {}

  StyleScopeFrame* GetParentFrameOrNull(Element& parent_element);
  StyleScopeFrame& GetParentFrameOrThis(Element& parent_element);

 private:
  friend class SelectorChecker;

  Element& element_;
  StyleScopeFrame* parent_;
  HeapHashMap<Member<const StyleScope>, Member<const StyleScopeActivations>>
      data_;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::StyleScopeActivation)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::StyleScopeFrame)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_FRAME_H_
