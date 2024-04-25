// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/window_name_collection.h"

#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"

namespace blink {

WindowNameCollection::WindowNameCollection(ContainerNode& document,
                                           const AtomicString& name)
    : HTMLNameCollection(document, kWindowNamedItems, name) {}

WindowNameCollection::WindowNameCollection(ContainerNode& document,
                                           CollectionType type,
                                           const AtomicString& name)
    : WindowNameCollection(document, name) {
  DCHECK_EQ(type, kWindowNamedItems);
}

bool WindowNameCollection::ElementMatches(const Element& element) const {
  // Match only images, forms, embeds and objects by name,
  // but anything by id
  if (IsA<HTMLImageElement>(element) || IsA<HTMLFormElement>(element) ||
      IsA<HTMLEmbedElement>(element) || IsA<HTMLObjectElement>(element)) {
    if (element.GetNameAttribute() == name_)
      return true;
  }
  return element.GetIdAttribute() == name_;
}

}  // namespace blink
