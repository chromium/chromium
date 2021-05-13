// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/screens.h"

#include "base/containers/contains.h"
#include "third_party/blink/public/common/widget/screen_info.h"
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
  UpdateScreenInfos(window, screen_infos);
}

const HeapVector<Member<ScreenAdvanced>>& Screens::screens() const {
  return screens_;
}

ScreenAdvanced* Screens::currentScreen() const {
  if (!DomWindow())
    return nullptr;

  if (screens_.IsEmpty())
    return nullptr;

  auto* it = base::ranges::find(screens_, current_display_id_,
                                &ScreenAdvanced::DisplayId);
  DCHECK(it != screens_.end());
  return *it;
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

void Screens::UpdateScreenInfos(LocalDOMWindow* window,
                                const ScreenInfos& new_infos) {
  // Expect that all updates contain a non-zero set of screens.
  DCHECK(!new_infos.screen_infos.empty());

  // (1) Detect if the set of screens has changed, and update screens_.
  bool added_or_removed = false;

  // O(displays) should be small, so O(n^2) check in both directions
  // instead of keeping some more efficient cache of display ids.

  // Check if any screens have been removed and remove them from screens_.
  for (WTF::wtf_size_t i = 0; i < screens_.size();
       /*conditionally incremented*/) {
    if (base::Contains(new_infos.screen_infos, screens_[i]->DisplayId(),
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
  for (const auto& info : new_infos.screen_infos) {
    if (!base::Contains(screens_, info.display_id,
                        &ScreenAdvanced::DisplayId)) {
      screens_.push_back(
          MakeGarbageCollected<ScreenAdvanced>(window, info.display_id));
      added_or_removed = true;
    }
  }

  // Update current_display_id_ so that currentScreen() is up to date
  // before we send out any events.
  current_display_id_ = new_infos.current_display_id;

  // (2) At this point, all data strutures are up to date.
  // screens_ has the current set of screens.
  // current_screen_ has new values pushed to it.
  // (prior to this function) individual ScreenAdvanced objects have new values.

  // (3) Send a change event if the current screen has changed.
  if (prev_screen_infos_.screen_infos.empty() ||
      prev_screen_infos_.current() != new_infos.current()) {
    DispatchEvent(*Event::Create(event_type_names::kCurrentscreenchange));
  }

  // (4) Send a change event if the set of screens has changed.
  if (added_or_removed) {
    DispatchEvent(*Event::Create(event_type_names::kScreenschange));
  }

  // (5) Send change events to individual screens if they have changed.
  // It's not guaranteed that screen_infos are ordered, so for each screen
  // find the info that corresponds to it in old_info and new_infos.
  for (const auto& screen : screens_) {
    auto id = screen->DisplayId();
    auto new_it =
        base::ranges::find(new_infos.screen_infos, id, &ScreenInfo::display_id);
    DCHECK(new_it != new_infos.screen_infos.end());
    auto old_it = base::ranges::find(prev_screen_infos_.screen_infos, id,
                                     &ScreenInfo::display_id);
    if (old_it != prev_screen_infos_.screen_infos.end() && *old_it != *new_it) {
      // TODO(enne): http://crbug.com/1202981 only send this event when
      // properties on ScreenAdvanced (vs anything in ScreenInfo) change.
      screen->DispatchEvent(*Event::Create(event_type_names::kChange));
    }
  }

  // (6) Store screen infos for change comparison next time.
  //
  // Aside: Because ScreenAdvanced is a "live" thin wrapper over the ScreenInfo
  // object owned by WidgetBase, WidgetBase's copy needs to be updated
  // in UpdateSurfaceAndScreenInfo prior to this UpdateScreenInfos call so that
  // when the events are fired, the live data is not stale.  Therefore, this
  // class needs to hold onto the "previous" info so that it knows which pieces
  // of data have changed, as at a higher level the old data has already been
  // rewritten with the new.
  prev_screen_infos_ = new_infos;
}

}  // namespace blink
