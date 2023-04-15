// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_KEY_HANDLING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_KEY_HANDLING_H_

namespace blink {

class Element;
class KeyboardEvent;

namespace css_toggle_key_handling {

// Handle keydown events that should have default behavior based on
// inferred roles for CSS toggles (see css_toggle_inference.h).
//
// Returns whether it handled the event.
bool HandleKeydownEvent(Element* element, KeyboardEvent& event);

}  // namespace css_toggle_key_handling

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_KEY_HANDLING_H_
