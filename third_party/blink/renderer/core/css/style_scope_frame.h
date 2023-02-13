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

class Element;
class StyleScope;
class SelectorChecker;

// The *activations* for a given StyleScope/element, is a list of active
// scopes found in the ancestor chain, their roots (Element*), and the
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

  // The root is the element when the activation happened. In other words,
  // the element that matched <scope-start>.
  //
  // https://drafts.csswg.org/css-cascade-6/#typedef-scope-start
  Member<Element> root;
  // The distance to the root, in terms of number of inclusive ancestors
  // between some subject element and the root.
  unsigned proximity = 0;
  // True if some subject element matches <scope-end>.
  //
  // https://drafts.csswg.org/css-cascade-6/#typedef-scope-end
  bool limit = false;
};

using StyleScopeActivations = HeapVector<StyleScopeActivation>;

// Stores the current @scope activations for a given subject element.
//
// See `StyleScopeActivation` for more information about activations.
class CORE_EXPORT StyleScopeFrame {
  STACK_ALLOCATED();

 public:
  explicit StyleScopeFrame(Element& element) : element_(element) {}

 private:
  friend class SelectorChecker;

  Element& element_;
  HeapHashMap<Member<const StyleScope>, Member<const StyleScopeActivations>>
      data_;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::StyleScopeActivation)
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::StyleScopeFrame)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SCOPE_FRAME_H_
