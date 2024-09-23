// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_keyboard_event.h"

namespace blink {

const size_t WebKeyboardEvent::kTextLengthCap;

std::unique_ptr<WebInputEvent> WebKeyboardEvent::Clone() const {
  return std::make_unique<WebKeyboardEvent>(*this);
}

bool WebKeyboardEvent::CanCoalesce(const WebInputEvent& event) const {
  return false;
}

void WebKeyboardEvent::Coalesce(const WebInputEvent& event) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace blink
