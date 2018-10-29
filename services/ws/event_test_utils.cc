// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/event_test_utils.h"

#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"

namespace ws {

std::string EventToEventType(const ui::Event* event) {
  if (!event)
    return "<null>";

  // TODO(sky): convert to using EventTypeName() is ui/events.h.
  switch (event->type()) {
    case ui::ET_CANCEL_MODE:
      return "CANCEL_MODE";

    case ui::ET_KEY_PRESSED:
      return "KEY_PRESSED";

    case ui::ET_MOUSE_DRAGGED:
      return "MOUSE_DRAGGED";
    case ui::ET_MOUSE_ENTERED:
      return "MOUSE_ENTERED";
    case ui::ET_MOUSE_MOVED:
      return "MOUSE_MOVED";
    case ui::ET_MOUSE_PRESSED:
      return "MOUSE_PRESSED";
    case ui::ET_MOUSE_RELEASED:
      return "MOUSE_RELEASED";
    default:
      break;
  }
  return std::string(EventTypeName(event->type()));
}

std::string LocatedEventToEventTypeAndLocation(const ui::Event* event) {
  if (!event)
    return "<null>";
  if (!event->IsLocatedEvent())
    return "<not-located-event>";
  return EventToEventType(event) + " " +
         event->AsLocatedEvent()->location().ToString();
}

}  // namespace ws
