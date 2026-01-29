// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_DEFER_PAGE_SWAP_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_DEFER_PAGE_SWAP_CONTROLLER_H_

#include <optional>

#include "third_party/blink/renderer/core/navigation_api/navigate_event.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class NavigationDeferPageSwapController final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit NavigationDeferPageSwapController(NavigateEvent* event)
      : navigate_event_(event) {}

  void addRestoreCallback(V8NavigationDeferPageSwapRestoreCallback* callback,
                          ExceptionState& exception_state) {
    navigate_event_->AddDeferPageSwapRestoreCallback(callback, exception_state);
  }

  void Trace(Visitor* visitor) const final {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(navigate_event_);
  }

 private:
  Member<NavigateEvent> navigate_event_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_DEFER_PAGE_SWAP_CONTROLLER_H_
