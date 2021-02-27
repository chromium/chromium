// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/screens.h"

#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/common/widget/screen_infos.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/screen_enumeration/screen_advanced.h"

namespace blink {

Screens::Screens(LocalDOMWindow* window)
    : ExecutionContextLifecycleObserver(window) {
  LocalFrame* frame = window->GetFrame();
  const auto& screen_infos = frame->GetChromeClient().GetScreenInfos(*frame);
  for (const auto& screen_info : screen_infos.screen_infos) {
    screens_.push_back(
        MakeGarbageCollected<ScreenAdvanced>(window, screen_info.display_id));
  }
}

const HeapVector<Member<ScreenAdvanced>>& Screens::screens() const {
  return screens_;
}

ScreenAdvanced* Screens::currentScreen() const {
  if (!DomWindow())
    return nullptr;

  LocalFrame* frame = DomWindow()->GetFrame();
  const auto& current_info = frame->GetChromeClient().GetScreenInfo(*frame);
  for (const auto& screen : screens_) {
    if (screen->DisplayId() == current_info.display_id)
      return screen;
  }

  NOTREACHED() << "No screen found matching the current ScreenInfo id";
  return nullptr;
}

const AtomicString& Screens::InterfaceName() const {
  return event_target_names::kScreens;
}

ExecutionContext* Screens::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void Screens::ContextDestroyed() {
  screens_.clear();
}

void Screens::Trace(Visitor* visitor) const {
  visitor->Trace(screens_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void Screens::ScreenInfosChanged() {
  // TODO(crbug.com/879300): Add or remove `screens_` members as needed. Fire
  // Screen.change instead of Screens.change for per-screen attribute changes.
  // This should not fire an event if exposed information has not changed.
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

}  // namespace blink
