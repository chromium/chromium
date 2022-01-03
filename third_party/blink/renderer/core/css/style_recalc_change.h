// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CHANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CHANGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Element;
class Node;
class PseudoElement;

// Class for keeping track of the need for traversing down flat tree children,
// recompute their computed styles, and marking nodes for layout tree re-
// attachment during the style recalc phase.
class CORE_EXPORT StyleRecalcChange {
 private:
  enum Flag {
    kNoFlags = 0,
    // Recalc container query dependent elements within this container,
    // but not in nested containers.
    kRecalcContainer = 1 << 0,
    // Recalc container query dependent elements within this container,
    // and also in nested containers.
    kRecalcDescendantContainers = 1 << 1,
    // If set, need to reattach layout tree.
    kReattach = 1 << 2,
    // If set, will prevent style recalc for the node passed to
    // ShouldRecalcStyleFor. This flag is lost when ForChildren is called.
    kSuppressRecalc = 1 << 3,
  };
  using Flags = uint8_t;

  static const Flags kRecalcContainerFlags =
      kRecalcContainer | kRecalcDescendantContainers;

 public:
  enum Propagate {
    // No need to update style of any children.
    kNo,
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
  StyleRecalcChange& operator=(const StyleRecalcChange&) = default;
  explicit StyleRecalcChange(Propagate propagate) : propagate_(propagate) {}

  bool IsEmpty() const { return !propagate_ && !flags_; }

  StyleRecalcChange ForChildren(const Element& element) const {
    return {RecalcDescendants() ? kRecalcDescendants : kNo,
            FlagsForChildren(element)};
  }
  StyleRecalcChange ForPseudoElement() const {
    if (propagate_ == kUpdatePseudoElements)
      return {kRecalcChildren, flags_};
    return *this;
  }
  StyleRecalcChange EnsureAtLeast(Propagate propagate) const {
    if (propagate > propagate_)
      return {propagate, flags_};
    return {propagate_, flags_};
  }
  StyleRecalcChange ForceRecalcDescendants() const {
    return {kRecalcDescendants, flags_};
  }
  StyleRecalcChange ForceReattachLayoutTree() const {
    return {propagate_, static_cast<Flags>(flags_ | kReattach)};
  }
  StyleRecalcChange ForceRecalcContainer() const {
    return {propagate_, static_cast<Flags>(flags_ | kRecalcContainer)};
  }
  StyleRecalcChange ForceRecalcDescendantContainers() const {
    return {propagate_,
            static_cast<Flags>(flags_ | kRecalcDescendantContainers)};
  }
  StyleRecalcChange SuppressRecalc() const {
    return {propagate_, static_cast<Flags>(flags_ | kSuppressRecalc)};
  }
  StyleRecalcChange WithRecalcContainerFlags(StyleRecalcChange& from) const {
    return {propagate_,
            static_cast<Flags>(flags_ | (from.flags_ & kRecalcContainerFlags))};
  }
  StyleRecalcChange Combine(const StyleRecalcChange& other) const {
    return {std::max(propagate_, other.propagate_),
            static_cast<Flags>(flags_ | other.flags_)};
  }

  bool ReattachLayoutTree() const { return flags_ & kReattach; }
  bool RecalcChildren() const { return propagate_ > kUpdatePseudoElements; }
  bool RecalcDescendants() const { return propagate_ == kRecalcDescendants; }
  bool UpdatePseudoElements() const { return propagate_ != kNo; }
  bool IndependentInherit() const { return propagate_ == kIndependentInherit; }
  bool TraverseChildren(const Element&) const;
  bool TraverseChild(const Node&) const;
  bool TraversePseudoElements(const Element&) const;
  bool ShouldRecalcStyleFor(const Node&) const;
  bool ShouldUpdatePseudoElement(const PseudoElement&) const;
  bool IsSuppressed() const { return flags_ & kSuppressRecalc; }

  String ToString() const;

 private:
  StyleRecalcChange(Propagate propagate, Flags flags)
      : propagate_(propagate), flags_(flags) {}

  bool RecalcContainerQueryDependent() const {
    return flags_ & kRecalcContainerFlags;
  }
  Flags FlagsForChildren(const Element&) const;

  // To what extent do we need to update style for children.
  Propagate propagate_ = kNo;
  // See StyleRecalc::Flag.
  Flags flags_ = kNoFlags;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RECALC_CHANGE_H_
