// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/user_media_request_provider.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

// static
const char UserMediaRequestProvider::kSupplementName[] =
    "UserMediaRequestProvider";

// static
UserMediaRequestProvider* UserMediaRequestProvider::From(
    LocalDOMWindow& window) {
  return Supplement<LocalDOMWindow>::From<UserMediaRequestProvider>(window);
}

UserMediaRequestProvider::UserMediaRequestProvider(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void UserMediaRequestProvider::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
