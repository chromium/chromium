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
  ScreenInfosChanged(window, screen_infos);
}

const HeapVector<Member<ScreenAdvanced>>& Screens::screens() const {
  return screens_;
}

ScreenAdvanced* Screens::currentScreen() const {
  if (!DomWindow())
    return nullptr;

  DCHECK(!screens_.IsEmpty());

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

void Screens::ScreenInfosChanged(LocalDOMWindow* window,
                                 const ScreenInfos& infos) {
  bool added_or_removed = false;

  // O(displays) should be small, so O(n^2) check in both directions
  // instead of keeping some more efficient cache of display ids.

  // Check if any screens have been removed and remove them from screens_.
  for (WTF::wtf_size_t i = 0; i < screens_.size();
       /*conditionally incremented*/) {
    if (base::Contains(infos.screen_infos, screens_[i]->DisplayId(),
                       &ScreenInfo::display_id)) {
      ++i;
    } else {
      screens_.EraseAt(i);
      added_or_removed = true;
      // Recheck this index.
    }
  }

  // Check if any screens have been added, and append them to the end of
  // screens_.
  for (const auto& info : infos.screen_infos) {
    if (!base::Contains(screens_, info.display_id,
                        &ScreenAdvanced::DisplayId)) {
      screens_.push_back(
          MakeGarbageCollected<ScreenAdvanced>(window, info.display_id));
      added_or_removed = true;
    }
  }

  if (added_or_removed) {
    DispatchEvent(*Event::Create(event_type_names::kChange));
  }
}

}  // namespace blink
