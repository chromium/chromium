// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_DATA_LIST_OPTIONS_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_DATA_LIST_OPTIONS_COLLECTION_H_

#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"

namespace blink {

class HTMLDataListOptionsCollection : public HTMLCollection {
 public:
  explicit HTMLDataListOptionsCollection(ContainerNode& owner_node)
      : HTMLCollection(owner_node,
                       kDataListOptions,
                       kDoesNotOverrideItemAfter) {}
  HTMLDataListOptionsCollection(ContainerNode& owner_node, CollectionType type)
      : HTMLDataListOptionsCollection(owner_node) {
    DCHECK_EQ(type, kDataListOptions);
  }

  HTMLOptionElement* Item(unsigned offset) const {
    return To<HTMLOptionElement>(HTMLCollection::item(offset));
  }

  bool ElementMatches(const HTMLElement&) const;
};

DEFINE_TYPE_CASTS(HTMLDataListOptionsCollection,
                  LiveNodeListBase,
                  collection,
                  collection->GetType() == kDataListOptions,
                  collection.GetType() == kDataListOptions);

inline bool HTMLDataListOptionsCollection::ElementMatches(
    const HTMLElement& element) const {
  return IsA<HTMLOptionElement>(element);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_DATA_LIST_OPTIONS_COLLECTION_H_
