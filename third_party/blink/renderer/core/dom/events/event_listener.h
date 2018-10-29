/*
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_LISTENER_H_

#include "third_party/blink/renderer/bindings/core/v8/custom_wrappable_adapter.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Event;
class ExecutionContext;

class CORE_EXPORT EventListener : public CustomWrappableAdapter {
 public:
  enum ListenerType {
    // |kJSEventListenerType| corresponds to EventListener defined in standard:
    // https://dom.spec.whatwg.org/#callbackdef-eventlistener
    kJSEventListenerType,
    // |kJSEventHandlerType| corresponds to EventHandler defined in standard:
    // https://html.spec.whatwg.org/multipage/webappapis.html#event-handler-attributes
    kJSEventHandlerType,
    // These are for C++ native callbacks.
    kImageEventListenerType,
    kCPPEventListenerType,
    kConditionEventListenerType,
  };

  ~EventListener() override = default;
  virtual bool operator==(const EventListener&) const = 0;
  virtual void handleEvent(ExecutionContext*, Event*) = 0;
  virtual const String& Code() const { return g_empty_string; }
  virtual bool IsEventHandlerForContentAttribute() const { return false; }
  virtual bool BelongsToTheCurrentWorld(ExecutionContext*) const {
    return false;
  }
  virtual bool IsEventHandler() const { return false; }

  ListenerType GetType() const { return type_; }

  // Returns true if this EventListener is implemented based on JS object.
  bool IsJSBased() const {
    return type_ == kJSEventListenerType || type_ == kJSEventHandlerType;
  }

  // Returns true if this EventListener is C++ native callback.
  bool IsNativeBased() const { return !IsJSBased(); }

  const char* NameInHeapSnapshot() const override { return "EventListener"; }

 protected:
  explicit EventListener(ListenerType type) : type_(type) {}

 private:
  ListenerType type_;
};

}  // namespace blink

#endif
