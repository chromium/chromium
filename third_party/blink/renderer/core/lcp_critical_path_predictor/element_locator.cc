// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"

#include "base/logging.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::element_locator {

absl::optional<ElementLocator> OfElement(Element* element) {
  ElementLocator locator;

  while (element) {
    Element* parent = element->parentElement();

    if (element->HasID()) {
      // Peg on element id if that exists

      ElementLocator_Component_Id* id_comp =
          locator.add_components()->mutable_id();
      id_comp->set_id_attr(element->GetIdAttribute().Utf8());
      break;
    } else if (parent) {
      // Last resort: n-th element that has the `tag_name`.

      AtomicString tag_name = element->localName();

      int nth = 0;
      for (Node* sibling = parent->firstChild(); sibling;
           sibling = sibling->nextSibling()) {
        Element* sibling_el = DynamicTo<Element>(sibling);
        if (!sibling_el || sibling_el->localName() != tag_name) {
          continue;
        }

        if (sibling_el == element) {
          ElementLocator_Component_NthTagName* nth_comp =
              locator.add_components()->mutable_nth();
          nth_comp->set_tag_name(tag_name.Utf8());
          nth_comp->set_index(nth);
          break;
        }

        ++nth;
      }
    }

    element = parent;
  }

  return locator;
}

String ToString(const ElementLocator& locator) {
  StringBuilder builder;

  for (const auto& c : locator.components()) {
    builder.Append('/');
    if (c.has_id()) {
      builder.Append('#');
      builder.Append(c.id().id_attr().c_str());
    } else if (c.has_nth()) {
      builder.Append(c.nth().tag_name().c_str());
      builder.Append('[');
      builder.AppendNumber(c.nth().index());
      builder.Append(']');
    } else {
      builder.Append("unknown_type");
    }
  }

  return builder.ReleaseString();
}

}  // namespace blink::element_locator
