// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_OWNER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_OWNER_ELEMENT_H_

#include "third_party/blink/renderer/core/html/forms/option_list.h"
#include "third_party/blink/renderer/core/html/forms/type_ahead.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

class HTMLMenuItemElement;

class CORE_EXPORT HTMLMenuOwnerElement : public HTMLElement,
                                         public TypeAheadDataSource {
 public:
  // This returns an iterable list of menuitems whose owner is this.
  MenuItemList ItemList() const;

  bool ShouldIgnoreDescendantsForElementTraversals(Element* element) const;
  bool IsTopLevelOwner() const;

  void DefaultEventHandler(Event&) override;

  void Trace(Visitor*) const override;

  // TypeAheadDataSource implementation
  int IndexOfSelectedOption() const override;
  int OptionCount() const override;
  String OptionAtIndex(int index) const override;

 protected:
  HTMLMenuOwnerElement(HTMLQualifiedName, Document&);

  TypeAhead type_ahead_;

 private:
  Member<HTMLMenuItemElement> last_mouseup_menu_item_;
  bool processing_click_ = false;
};

template <>
struct DowncastTraits<HTMLMenuOwnerElement> {
  static bool AllowFrom(const Node& node) {
    return node.HasTagName(html_names::kMenulistTag) ||
           node.HasTagName(html_names::kMenubarTag);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_OWNER_ELEMENT_H_
