// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/document_all_name_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

DocumentAllNameCollection::DocumentAllNameCollection(ContainerNode& document,
                                                     const AtomicString& name)
    : HTMLNameCollection(document, kDocumentAllNamedItems, name) {}

DocumentAllNameCollection::DocumentAllNameCollection(ContainerNode& document,
                                                     CollectionType type,
                                                     const AtomicString& name)
    : DocumentAllNameCollection(document, name) {
  DCHECK_EQ(type, kDocumentAllNamedItems);
}

bool DocumentAllNameCollection::ElementMatches(const Element& element) const {
  // https://html.spec.whatwg.org/C/#all-named-elements
  // Match below type of elements by name but any type of element by id.
  if (element.HasTagName(html_names::kATag) ||
      element.HasTagName(html_names::kButtonTag) ||
      element.HasTagName(html_names::kEmbedTag) ||
      element.HasTagName(html_names::kFormTag) ||
      element.HasTagName(html_names::kFrameTag) ||
      element.HasTagName(html_names::kFramesetTag) ||
      element.HasTagName(html_names::kIFrameTag) ||
      element.HasTagName(html_names::kImgTag) ||
      element.HasTagName(html_names::kInputTag) ||
      element.HasTagName(html_names::kMapTag) ||
      element.HasTagName(html_names::kMetaTag) ||
      element.HasTagName(html_names::kObjectTag) ||
      element.HasTagName(html_names::kSelectTag) ||
      element.HasTagName(html_names::kTextareaTag)) {
    if (element.GetNameAttribute() == name_)
      return true;
  }

  return element.GetIdAttribute() == name_;
}

}  // namespace blink
