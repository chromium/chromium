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

#include "third_party/blink/renderer/core/events/ui_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"

namespace blink {

UIEvent::UIEvent() : detail_(0) {}

UIEvent::UIEvent(const AtomicString& event_type,
                 Bubbles bubbles,
                 Cancelable cancelable,
                 ComposedMode composed_mode,
                 base::TimeTicks platform_time_stamp,
                 AbstractView* view_arg,
                 int detail_arg,
                 InputDeviceCapabilities* source_capabilities_arg)
    : Event(event_type,
            bubbles,
            cancelable,
            composed_mode,
            platform_time_stamp),
      view_(view_arg),
      detail_(detail_arg),
      source_capabilities_(source_capabilities_arg) {}

UIEvent::UIEvent(const AtomicString& event_type,
                 const UIEventInit* initializer,
                 base::TimeTicks platform_time_stamp)
    : Event(event_type, initializer, platform_time_stamp),
      view_(initializer->view()),
      detail_(initializer->detail()),
      source_capabilities_(initializer->sourceCapabilities()) {}

UIEvent::~UIEvent() = default;

void UIEvent::initUIEvent(const AtomicString& type_arg,
                          bool bubbles_arg,
                          bool cancelable_arg,
                          AbstractView* view_arg,
                          int detail_arg) {
  InitUIEventInternal(type_arg, bubbles_arg, cancelable_arg, nullptr, view_arg,
                      detail_arg, nullptr);
}

void UIEvent::InitUIEventInternal(
    const AtomicString& type_arg,
    bool bubbles_arg,
    bool cancelable_arg,
    EventTarget* related_target,
    AbstractView* view_arg,
    int detail_arg,
    InputDeviceCapabilities* source_capabilities_arg) {
  if (IsBeingDispatched())
    return;

  initEvent(type_arg, bubbles_arg, cancelable_arg, related_target);

  view_ = view_arg;
  detail_ = detail_arg;
  source_capabilities_ = source_capabilities_arg;
}

bool UIEvent::IsUIEvent() const {
  return true;
}

const AtomicString& UIEvent::InterfaceName() const {
  return event_interface_names::kUIEvent;
}

unsigned UIEvent::which() const {
  return 0;
}

void UIEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(view_);
  visitor->Trace(source_capabilities_);
  Event::Trace(visitor);
}

}  // namespace blink
