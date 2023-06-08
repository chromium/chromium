// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"

#include "base/logging.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String ToElementLocatorString(Element* element) {
  StringBuilder builder;

  while (element) {
    Element* parent = element->parentElement();

    if (element->HasID()) {
      // Peg on element id if that exists

      builder.Append("/#");
      builder.Append(element->GetIdAttribute());
      break;
    } else if (parent) {
      // Last resort: n-th element that has tagName.

      String tag_name = element->tagName();

      int nth = 0;
      for (Node* sibling = parent->firstChild(); sibling;
           sibling = sibling->nextSibling()) {
        Element* sibling_el = DynamicTo<Element>(sibling);
        if (!sibling_el || sibling_el->tagName() != tag_name) {
          continue;
        }

        if (sibling_el == element) {
          builder.Append('/');
          builder.Append(tag_name);
          builder.Append('[');
          builder.AppendNumber(nth);
          builder.Append(']');

          break;
        }

        ++nth;
      }
    }

    element = parent;
  }

  return builder.ReleaseString();
}

}  // namespace blink
