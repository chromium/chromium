// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class Element;
class HTMLSlotElement;

// StyleRecalcContext is an object that is passed on the stack during
// the style recalc process.
//
// Its purpose is to hold context related to the style recalc process as
// a whole, i.e. information not directly associated to the specific element
// style is being calculated for.
class CORE_EXPORT StyleRecalcContext {
  STACK_ALLOCATED();

 public:
  // Using the ancestor chain, build a StyleRecalcContext suitable for
  // resolving the style of the given Element.
  static StyleRecalcContext FromAncestors(Element&);

  // If the passed in StyleRecalcContext is nullptr, build a StyleRecalcContext
  // suitable for resolving the style for child elements of the passed in
  // element. Otherwise return the passed in context as a value.
  static StyleRecalcContext FromInclusiveAncestors(Element&);

  // When traversing into slotted children, the container is in the shadow-
  // including inclusive ancestry of the slotted element's host. Return a
  // context with the container adjusted as necessary.
  StyleRecalcContext ForSlotChildren(const HTMLSlotElement& slot) const;

  // Called to update the context when matching ::slotted rules for shadow host
  // children. ::slotted rules may query containers inside the slot's shadow
  // tree as well.
  StyleRecalcContext ForSlottedRules(HTMLSlotElement& slot) const;

  // Called to update the context when matching ::part rules for shadow hosts.
  StyleRecalcContext ForPartRules(Element& host) const;

  // Set to the nearest container (for container queries), if any.
  // This is used to evaluate container queries in ElementRuleCollector.
  Element* container = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CONTEXT_H_
