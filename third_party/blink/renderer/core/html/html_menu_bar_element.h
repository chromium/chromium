// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_BAR_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_BAR_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_menu_owner_element.h"

namespace blink {

class CORE_EXPORT HTMLMenuBarElement final : public HTMLMenuOwnerElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLMenuBarElement(Document&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MENU_BAR_ELEMENT_H_
