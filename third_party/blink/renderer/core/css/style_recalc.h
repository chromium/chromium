// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ContainerQueryEvaluator;
class Element;
class Node;
class PseudoElement;

// Class for keeping track of the need for traversing down flat tree children,
// recompute their computed styles, and marking nodes for layout tree re-
// attachment during the style recalc phase.
class StyleRecalcChange {
 public:
  enum Propagate {
    // No need to update style of any children.
    kNo,
    // Need to traverse children in display:none or non-slotted/distributed
    // children of shadow hosts to clear ensured computed styles.
    kClearEnsured,
    // Need to traverse descendants to invalidate style for container queries.
    // This value is passed in for the container itself, it will translate into
    // recalc_container_query_dependent_=true for descendants. We should not
    // recalc style for the container itself.
    kRecalcContainerQueryDependent,
    // Need to update existence and style for pseudo elements.
    kUpdatePseudoElements,
    // Need to recalculate style for children for inheritance. All changed
    // inherited properties can be propagated (PropagateInheritedProperties)
    // instead of a full rule matching.
    kIndependentInherit,
    // Need to recalculate style for children, typically for inheritance.
    kRecalcChildren,
    // Need to recalculate style for all descendants.
    kRecalcDescendants,
  };

  StyleRecalcChange() = default;
  StyleRecalcChange(const StyleRecalcChange&) = default;
  StyleRecalcChange(Propagate propagate) : propagate_(propagate) {}

  StyleRecalcChange ForChildren(const Element& element) const {
    return {RecalcDescendants() ? kRecalcDescendants : kNo, reattach_,
            RecalcContainerQueryDependentChildren(element)};
  }
  StyleRecalcChange ForPseudoElement() const {
    if (propagate_ == kUpdatePseudoElements)
      return {kRecalcChildren, reattach_, recalc_container_query_dependent_};
    return *this;
  }
  StyleRecalcChange EnsureAtLeast(Propagate propagate) const {
    if (propagate > propagate_)
      return {propagate, reattach_, recalc_container_query_dependent_};
    return {propagate_, reattach_, recalc_container_query_dependent_};
  }
  StyleRecalcChange ForceRecalcDescendants() const {
    return {kRecalcDescendants, reattach_, recalc_container_query_dependent_};
  }
  StyleRecalcChange ForceReattachLayoutTree() const {
    return {propagate_, true, recalc_container_query_dependent_};
  }

  bool ReattachLayoutTree() const { return reattach_; }
  bool RecalcChildren() const { return propagate_ > kUpdatePseudoElements; }
  bool RecalcDescendants() const { return propagate_ == kRecalcDescendants; }
  bool UpdatePseudoElements() const { return propagate_ != kNo; }
  bool IndependentInherit() const { return propagate_ == kIndependentInherit; }
  bool TraverseChildren(const Element&) const;
  bool TraverseChild(const Node&) const;
  bool TraversePseudoElements(const Element&) const;
  bool ShouldRecalcStyleFor(const Node&) const;
  bool ShouldUpdatePseudoElement(const PseudoElement&) const;

 private:
  StyleRecalcChange(Propagate propagate,
                    bool reattach,
                    bool recalc_container_query_dependent)
      : propagate_(propagate),
        reattach_(reattach),
        recalc_container_query_dependent_(recalc_container_query_dependent) {}

  bool RecalcContainerQueryDependent() const {
    return recalc_container_query_dependent_;
  }
  bool RecalcContainerQueryDependentChildren(const Element&) const;

  // To what extent do we need to update style for children.
  Propagate propagate_ = kNo;
  // Need to reattach layout tree if true.
  bool reattach_ = false;
  // Force recalc of elements depending on container queries.
  bool recalc_container_query_dependent_ = false;
};

// StyleRecalcContext is an object that is passed on the stack during
// the style recalc process.
//
// Its purpose is to hold context related to the style recalc process as
// a whole, i.e. information not directly associated to the specific element
// style is being calculated for.
class StyleRecalcContext {
  STACK_ALLOCATED();

 public:
  // If style is being calculated for an element inside a container,
  // this ContainerQueryEvaluator may be used to evaluate @container
  // rules against that container.
  ContainerQueryEvaluator* cq_evaluator = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_H_
