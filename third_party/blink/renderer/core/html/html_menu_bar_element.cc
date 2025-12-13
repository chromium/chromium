// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_bar_element.h"

#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLMenuBarElement::HTMLMenuBarElement(Document& document)
    : HTMLMenuOwnerElement(html_names::kMenubarTag, document) {}

}  // namespace blink
