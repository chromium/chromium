// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusgroup_dom_token_list.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

FocusgroupDOMTokenList::FocusgroupDOMTokenList(Element& element)
    : DOMTokenList(element, html_names::kFocusgroupAttr) {}

bool FocusgroupDOMTokenList::ValidateTokenValue(const AtomicString& token_value,
                                                ExceptionState&) const {
  return focusgroup::IsValidFocusgroupToken(token_value);
}

}  // namespace blink
