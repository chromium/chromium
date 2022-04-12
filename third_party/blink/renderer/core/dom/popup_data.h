// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPUP_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPUP_DATA_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class PopupData final : public GarbageCollected<PopupData> {
 public:
  PopupData() = default;
  PopupData(const PopupData&) = delete;
  PopupData& operator=(const PopupData&) = delete;

  bool open() const { return open_; }
  void setOpen(bool open) { open_ = open; }

  bool hadInitiallyOpenWhenParsed() const {
    return had_initiallyopen_when_parsed_;
  }
  void setHadInitiallyOpenWhenParsed(bool value) {
    had_initiallyopen_when_parsed_ = value;
  }

  PopupValueType type() const { return type_; }
  void setType(PopupValueType type) {
    type_ = type;
    DCHECK_NE(type, PopupValueType::kNone)
        << "Remove PopupData rather than setting kNone type";
  }

  Element* invoker() const { return invoker_; }
  void setInvoker(Element* element) { invoker_ = element; }

  void setNeedsRepositioningForSelectMenu(bool flag) {
    needs_repositioning_for_select_menu_ = flag;
  }
  bool needsRepositioningForSelectMenu() {
    return needs_repositioning_for_select_menu_;
  }

  HTMLSelectMenuElement* ownerSelectMenuElement() const {
    return owner_select_menu_element_;
  }
  void setOwnerSelectMenuElement(HTMLSelectMenuElement* element) {
    owner_select_menu_element_ = element;
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(invoker_);
    visitor->Trace(owner_select_menu_element_);
  }

 private:
  bool open_ = false;
  bool had_initiallyopen_when_parsed_ = false;
  PopupValueType type_ = PopupValueType::kNone;
  WeakMember<Element> invoker_;

  // TODO(crbug.com/1197720): The popup position should be provided by the new
  // anchored positioning scheme.
  bool needs_repositioning_for_select_menu_ = false;
  WeakMember<HTMLSelectMenuElement> owner_select_menu_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPUP_DATA_H_
