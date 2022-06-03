/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/events/clipboard_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_clipboard_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

ClipboardEvent::ClipboardEvent(const AtomicString& type,
                               DataTransfer* clipboard_data)
    : Event(type, Bubbles::kYes, Cancelable::kYes, ComposedMode::kComposed),
      clipboard_data_(clipboard_data) {}

ClipboardEvent::ClipboardEvent(const AtomicString& type,
                               const ClipboardEventInit* initializer)
    : Event(type, initializer), clipboard_data_(initializer->clipboardData()) {}

ClipboardEvent::~ClipboardEvent() = default;

const AtomicString& ClipboardEvent::InterfaceName() const {
  return event_interface_names::kClipboardEvent;
}

bool ClipboardEvent::IsClipboardEvent() const {
  return true;
}

void ClipboardEvent::Trace(Visitor* visitor) const {
  visitor->Trace(clipboard_data_);
  Event::Trace(visitor);
}

}  // namespace blink
