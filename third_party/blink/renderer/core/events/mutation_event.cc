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

#include "third_party/blink/renderer/core/events/mutation_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

MutationEvent::MutationEvent() : attr_change_(0) {}

MutationEvent::MutationEvent(const AtomicString& type,
                             Bubbles bubbles,
                             Cancelable cancelable,
                             Node* related_node,
                             const String& prev_value,
                             const String& new_value,
                             const String& attr_name,
                             uint16_t attr_change)
    : Event(type, bubbles, cancelable),
      related_node_(related_node),
      prev_value_(prev_value),
      new_value_(new_value),
      attr_name_(attr_name),
      attr_change_(attr_change) {}

MutationEvent::~MutationEvent() = default;

void MutationEvent::initMutationEvent(const AtomicString& type,
                                      bool bubbles,
                                      bool cancelable,
                                      Node* related_node,
                                      const String& prev_value,
                                      const String& new_value,
                                      const String& attr_name,
                                      uint16_t attr_change) {
  if (IsBeingDispatched())
    return;

  initEvent(type, bubbles, cancelable);

  related_node_ = related_node;
  prev_value_ = prev_value;
  new_value_ = new_value;
  attr_name_ = attr_name;
  attr_change_ = attr_change;
}

const AtomicString& MutationEvent::InterfaceName() const {
  return event_interface_names::kMutationEvent;
}

void MutationEvent::Trace(Visitor* visitor) const {
  visitor->Trace(related_node_);
  Event::Trace(visitor);
}

}  // namespace blink
