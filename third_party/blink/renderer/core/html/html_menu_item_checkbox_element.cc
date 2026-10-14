// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_item_checkbox_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLMenuItemCheckboxElement::HTMLMenuItemCheckboxElement(Document& document)
    : HTMLMenuItemCheckableElementBase(html_names::kMenuitemcheckboxTag,
                                       document) {}

HTMLMenuItemCheckboxElement::~HTMLMenuItemCheckboxElement() = default;

}  // namespace blink
