// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECTED_CONTENT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECTED_CONTENT_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class HTMLOptionElement;

class HTMLSelectedContentElement : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLSelectedContentElement(Document&);

  bool IsDisabled() const { return disabled_; }

  ElementType GetElementType() const final {
    return ElementType::kHTMLSelectedContentElement;
  }

  // CloneContentsFromOptionElement clones the contents of a single option
  // element into this selectedcontent element, which is used for select
  // elements without the multiple attribute.
  void CloneContentsFromOptionElement(const HTMLOptionElement*);
  // CloneMultipleOptionsFromSelectElement clones the contents of each selected
  // option element into this selectedcontent element, which is used for select
  // elements with the multiple attribute.
  void CloneMultipleOptionsFromSelectElement(HTMLSelectElement&);

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void DidNotifySubtreeInsertionsToDocument() override;
  void RemovedFrom(ContainerNode&) override;

  void Trace(Visitor*) const override;

 private:
  // When this is true, cloning is disabled.
  bool disabled_ = false;

  Member<HTMLSelectElement> nearest_ancestor_select_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_SELECTED_CONTENT_ELEMENT_H_
