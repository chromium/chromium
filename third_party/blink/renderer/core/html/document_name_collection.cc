// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/document_name_collection.h"

#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"

namespace blink {

DocumentNameCollection::DocumentNameCollection(ContainerNode& document,
                                               const AtomicString& name)
    : HTMLNameCollection(document, kDocumentNamedItems, name) {}

DocumentNameCollection::DocumentNameCollection(ContainerNode& document,
                                               CollectionType type,
                                               const AtomicString& name)
    : DocumentNameCollection(document, name) {
  DCHECK_EQ(type, kDocumentNamedItems);
}

// https://html.spec.whatwg.org/C/#dom-document-nameditem-filter
bool DocumentNameCollection::ElementMatches(const HTMLElement& element) const {
  // Match images, forms, embeds, objects and iframes by name,
  // object by id, and images by id but only if they have
  // a name attribute (this very strange rule matches IE)
  auto* html_embed_element = DynamicTo<HTMLEmbedElement>(&element);
  if (IsA<HTMLFormElement>(element) || IsA<HTMLIFrameElement>(element) ||
      (html_embed_element && html_embed_element->IsExposed()))
    return element.GetNameAttribute() == name_;

  auto* html_image_element = DynamicTo<HTMLObjectElement>(&element);
  if (html_image_element && html_image_element->IsExposed())
    return element.GetNameAttribute() == name_ ||
           element.GetIdAttribute() == name_;
  if (IsA<HTMLImageElement>(element)) {
    const AtomicString& name_value = element.GetNameAttribute();
    return name_value == name_ ||
           (element.GetIdAttribute() == name_ && !name_value.empty());
  }
  return false;
}

}  // namespace blink
