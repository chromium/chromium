// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_H_

namespace blink {

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

  StyleRecalcChange ForChildren() const {
    return {RecalcDescendants() ? kRecalcDescendants : kNo, reattach_,
            calc_invisible_};
  }
  StyleRecalcChange ForPseudoElement() const {
    if (propagate_ == kUpdatePseudoElements)
      return {kRecalcChildren, reattach_, calc_invisible_};
    return *this;
  }
  StyleRecalcChange EnsureAtLeast(Propagate propagate) const {
    if (propagate > propagate_)
      return {propagate, reattach_, calc_invisible_};
    return {propagate_, reattach_, calc_invisible_};
  }
  StyleRecalcChange ForceRecalcDescendants() const {
    return {kRecalcDescendants, reattach_, calc_invisible_};
  }
  StyleRecalcChange ForceReattachLayoutTree() const {
    return {propagate_, true, calc_invisible_};
  }
  StyleRecalcChange ForceCalcInvisible() const {
    return {propagate_, reattach_, true};
  }

  bool ReattachLayoutTree() const { return reattach_; }
  bool RecalcChildren() const { return propagate_ > kUpdatePseudoElements; }
  bool RecalcDescendants() const { return propagate_ == kRecalcDescendants; }
  bool UpdatePseudoElements() const { return propagate_ != kNo; }
  bool IndependentInherit() const { return propagate_ == kIndependentInherit; }
  bool TraverseChildren(const Node&) const;
  bool TraverseChild(const Node&) const;
  bool TraversePseudoElements(const Node&) const;
  bool ShouldRecalcStyleFor(const Node&) const;
  bool ShouldUpdatePseudoElement(const PseudoElement&) const;
  bool CalcInvisible() const { return calc_invisible_; }

 private:
  StyleRecalcChange(Propagate propagate, bool reattach, bool calc_invisible)
      : propagate_(propagate),
        reattach_(reattach),
        calc_invisible_(calc_invisible) {}

  // To what extent do we need to update style for children.
  Propagate propagate_ = kNo;
  // Need to reattach layout tree if true.
  bool reattach_ = false;
  // Forcing ComputedStyle for find-in-page for invisible DOM.
  bool calc_invisible_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_H_
