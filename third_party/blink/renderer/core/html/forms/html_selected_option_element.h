// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECTED_OPTION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECTED_OPTION_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class HTMLSelectElement;
class HTMLSelectedOptionElement;
class HTMLOptionElement;

// TODO(crbug.com/1511354): SynchronousMutationObserver, especially with the
// IsDescendantOf check, is slow. Some ideas to speed things up:
// - Only create a mutation observer if the <selectedoption> isConnected.
// - Only run checks when the <selectedoption> is included in the flat tree.
// - Only create a mutation observer if the <select> is in appearance:base-select
//   mode.
// - Only create one mutation observer for each <selectedoption> in a <select>,
//   which may happen due to having one in the <select>'s UA shadowroot.
// - Avoid calling HTMLSelectElement::SelectedOption which iterates over all of
//   the <select>'s <option>s.
// - See if SVGUseElement has any strategies which could be employed here.
// - Create a new type of MutationObserver which only runs for certain subtrees,
//   which would allow us to reuse the existing tree walks during
//   insertion/removal.
// - Use the external asynchronous MutationObserver API, which would also be
//   easier to spec.
class SelectedOptionMutationObserver
    : public GarbageCollected<SelectedOptionMutationObserver>,
      public SynchronousMutationObserver {
 public:
  explicit SelectedOptionMutationObserver(
      HTMLSelectedOptionElement* selectedoption);

  void ChildrenOrAttributeChanged(const ContainerNode& node);

  void DidChangeChildren(const ContainerNode& container,
                         const ContainerNode::ChildrenChange& change) override;
  void AttributeChanged(const Element& target,
                        const QualifiedName& name,
                        const AtomicString& old_value,
                        const AtomicString& new_value) override;
  void Trace(Visitor* visitor) const override;

 private:
  Member<HTMLSelectedOptionElement> selectedoption_;
};

class HTMLSelectedOptionElement : public HTMLElement {
 public:
  explicit HTMLSelectedOptionElement(Document&);

  HTMLSelectElement* FirstAncestorSelectElement() const;
  void UpdateFirstAncestorSelectElement();

  HTMLOptionElement* SelectedOptionElement() const {
    return selected_option_element_;
  }
  void SelectedOptionElementChanged(HTMLOptionElement* option);

  void CloneContentsFromOptionElement();

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  void Trace(Visitor*) const override;

 private:
  void CreateOrDeleteMutationObserver();

  Member<HTMLSelectElement> first_ancestor_select_element_;
  Member<HTMLOptionElement> selected_option_element_;
  Member<SelectedOptionMutationObserver> mutation_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECTED_OPTION_ELEMENT_H_
