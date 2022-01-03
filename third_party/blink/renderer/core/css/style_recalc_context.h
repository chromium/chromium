// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CONTEXT_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class Element;

// StyleRecalcContext is an object that is passed on the stack during
// the style recalc process.
//
// Its purpose is to hold context related to the style recalc process as
// a whole, i.e. information not directly associated to the specific element
// style is being calculated for.
class StyleRecalcContext {
  STACK_ALLOCATED();

 public:
  // Using the ancestor chain, build a StyleRecalcContext suitable for
  // resolving the style of the given Element.
  static StyleRecalcContext FromAncestors(Element&);

  // If the passed in StyleRecalcContext is nullptr, build a StyleRecalcContext
  // suitable for resolving the style for child elements of the passed in
  // element. Otherwise return the passed in context as a value.
  static StyleRecalcContext FromInclusiveAncestors(Element&);

  // Set to the nearest container (for container queries), if any.
  // This is used to evaluate container queries in ElementRuleCollector.
  Element* container = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CONTEXT_H_
