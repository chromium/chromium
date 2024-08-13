// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_details/screen_details.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/screen_details/screen_detailed.h"
#include "ui/display/screen_info.h"

namespace blink {

ScreenDetails::ScreenDetails(LocalDOMWindow* window)
    : ExecutionContextLifecycleObserver(window) {
  LocalFrame* frame = window->GetFrame();
  const auto& screen_infos = frame->GetChromeClient().GetScreenInfos(*frame);
  // Do not dispatch change events during class initialization.
  UpdateScreenInfosImpl(window, screen_infos, /*dispatch_events=*/false);
}

const HeapVector<Member<ScreenDetailed>>& ScreenDetails::screens() const {
  return screens_;
}

ScreenDetailed* ScreenDetails::currentScreen() const {
  if (!DomWindow())
    return nullptr;

  if (screens_.empty())
    return nullptr;

  auto it = base::ranges::find(screens_, current_display_id_,
                               &ScreenDetailed::DisplayId);
  CHECK(it != screens_.end(), base::NotFatalUntil::M130);
  return it->Get();
}

const AtomicString& ScreenDetails::InterfaceName() const {
  return event_target_names::kScreenDetails;
}

ExecutionContext* ScreenDetails::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void ScreenDetails::ContextDestroyed() {
  screens_.clear();
}

void ScreenDetails::Trace(Visitor* visitor) const {
  visitor->Trace(screens_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void ScreenDetails::UpdateScreenInfos(LocalDOMWindow* window,
                                      const display::ScreenInfos& new_infos) {
  UpdateScreenInfosImpl(window, new_infos, /*dispatch_events=*/true);
}

void ScreenDetails::UpdateScreenInfosImpl(LocalDOMWindow* window,
                                          const display::ScreenInfos& new_infos,
                                          bool dispatch_events) {
  // Expect that all updates contain a non-zero set of screens.
  DCHECK(!new_infos.screen_infos.empty());

  // (1) Detect if screens were added or removed and update web exposed data.
  bool added_or_removed = false;

  // O(displays) should be small, so O(n^2) check in both directions
  // instead of keeping some more efficient cache of display ids.

  // Check if any screens have been removed and remove them from `screens_`.
  for (WTF::wtf_size_t i = 0; i < screens_.size();
       /*conditionally incremented*/) {
    if (base::Contains(new_infos.screen_infos, screens_[i]->DisplayId(),
                       &display::ScreenInfo::display_id)) {
      ++i;
    } else {
      screens_.EraseAt(i);
      added_or_removed = true;
      // Recheck this index.
    }
  }

  // Check if any screens have been added, and append them to `screens_`.
  for (const auto& info : new_infos.screen_infos) {
    if (!base::Contains(screens_, info.display_id,
                        &ScreenDetailed::DisplayId)) {
      screens_.push_back(
          MakeGarbageCollected<ScreenDetailed>(window, info.display_id));
      added_or_removed = true;
    }
  }

  // Sort `screens_` by position; x first and then y.
  base::ranges::stable_sort(screens_, [](ScreenDetailed* a, ScreenDetailed* b) {
    if (a->left() != b->left())
      return a->left() < b->left();
    return a->top() < b->top();
  });

  // Update current_display_id_ for currentScreen() before event dispatch.
  current_display_id_ = new_infos.current_display_id;

  // (2) At this point, all web exposed data is updated.
  // `screens_` has the updated set of screens.
  // `current_display_id_` has the updated value.
  // (prior to this function) individual ScreenDetailed objects have new values.
  //
  // Enqueue events for dispatch if `dispatch_events` is true.
  // Enqueuing event dispatch avoids recursion if screen changes occur while an
  // event handler is running a nested event loop, e.g. via window.print().
  if (dispatch_events) {
    // Enqueue a change event if the current screen has changed.
    if (prev_screen_infos_.screen_infos.empty() ||
        prev_screen_infos_.current().display_id !=
            new_infos.current().display_id ||
        !ScreenDetailed::AreWebExposedScreenDetailedPropertiesEqual(
            prev_screen_infos_.current(), new_infos.current())) {
      EnqueueEvent(*Event::Create(event_type_names::kCurrentscreenchange),
                   TaskType::kMiscPlatformAPI);
    }

    // Enqueue a change event if screens were added or removed.
    if (added_or_removed) {
      EnqueueEvent(*Event::Create(event_type_names::kScreenschange),
                   TaskType::kMiscPlatformAPI);
    }

    // Enqueue change events for any individual screens that changed.
    // It's not guaranteed that screen_infos are ordered, so for each screen
    // find the info that corresponds to it in old_info and new_infos.
    for (Member<ScreenDetailed>& screen : screens_) {
      auto id = screen->DisplayId();
      auto new_it = base::ranges::find(new_infos.screen_infos, id,
                                       &display::ScreenInfo::display_id);
      CHECK(new_it != new_infos.screen_infos.end(), base::NotFatalUntil::M130);
      auto old_it = base::ranges::find(prev_screen_infos_.screen_infos, id,
                                       &display::ScreenInfo::display_id);
      if (old_it != prev_screen_infos_.screen_infos.end() &&
          !ScreenDetailed::AreWebExposedScreenDetailedPropertiesEqual(
              *old_it, *new_it)) {
        screen->EnqueueEvent(*Event::Create(event_type_names::kChange),
                             TaskType::kMiscPlatformAPI);
      }
    }
  }

  // (3) Store screen infos for change comparison next time.
  //
  // Aside: Because ScreenDetailed is a "live" thin wrapper over the ScreenInfo
  // object owned by WidgetBase, WidgetBase's copy needs to be updated
  // in UpdateSurfaceAndScreenInfo prior to this UpdateScreenInfos call so that
  // when the events are fired, the live data is not stale.  Therefore, this
  // class needs to hold onto the "previous" info so that it knows which pieces
  // of data have changed, as at a higher level the old data has already been
  // rewritten with the new.
  prev_screen_infos_ = new_infos;
}

}  // namespace blink
