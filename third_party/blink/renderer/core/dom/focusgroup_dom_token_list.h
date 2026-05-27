// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_DOM_TOKEN_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_DOM_TOKEN_LIST_H_

#include "third_party/blink/renderer/core/dom/dom_token_list.h"

namespace blink {

class Element;

// DOMTokenList subclass for the focusgroup attribute. Overrides
// ValidateTokenValue to define the set of supported tokens for
// DOMTokenList.supports() feature detection.
class FocusgroupDOMTokenList final : public DOMTokenList {
 public:
  explicit FocusgroupDOMTokenList(Element& element);

 private:
  bool ValidateTokenValue(const AtomicString&, ExceptionState&) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSGROUP_DOM_TOKEN_LIST_H_
