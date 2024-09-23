/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/dom/events/event_listener_map.h"

#include "base/bits.h"
#include "base/debug/crash_logging.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_options.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#if DCHECK_IS_ON()
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#endif

namespace blink {

EventListenerMap::EventListenerMap() = default;

bool EventListenerMap::Contains(const AtomicString& event_type) const {
  for (const auto& entry : entries_) {
    if (entry.first == event_type)
      return true;
  }
  return false;
}

bool EventListenerMap::ContainsCapturing(const AtomicString& event_type) const {
  for (const auto& entry : entries_) {
    if (entry.first == event_type) {
      for (const auto& event_listener : *entry.second) {
        if (event_listener->Capture()) {
          return true;
        }
      }
      return false;
    }
  }
  return false;
}

bool EventListenerMap::ContainsJSBasedEventListeners(
    const AtomicString& event_type) const {
  for (const auto& entry : entries_) {
    if (entry.first == event_type) {
      for (const auto& event_listener : *entry.second) {
        const EventListener* callback = event_listener->Callback();
        if (callback && callback->IsJSBasedEventListener())
          return true;
      }
      return false;
    }
  }
  return false;
}

void EventListenerMap::Clear() {
  for (const auto& entry : entries_) {
    for (const auto& registered_listener : *entry.second) {
      registered_listener->SetRemoved();
    }
  }
  entries_.clear();
}

Vector<AtomicString> EventListenerMap::EventTypes() const {
  Vector<AtomicString> types;
  types.ReserveInitialCapacity(entries_.size());

  for (const auto& entry : entries_)
    types.UncheckedAppend(entry.first);

  return types;
}

static bool AddListenerToVector(EventListenerVector* listener_vector,
                                EventListener* listener,
                                const AddEventListenerOptionsResolved* options,
                                RegisteredEventListener** registered_listener) {
  for (auto& item : *listener_vector) {
    if (item->Matches(listener, options)) {
      // Duplicate listener.
      return false;
    }
  }

  *registered_listener =
      MakeGarbageCollected<RegisteredEventListener>(listener, options);
  listener_vector->push_back(*registered_listener);
  return true;
}

bool EventListenerMap::Add(const AtomicString& event_type,
                           EventListener* listener,
                           const AddEventListenerOptionsResolved* options,
                           RegisteredEventListener** registered_listener) {
  for (const auto& entry : entries_) {
    if (entry.first == event_type) {
      // Report the size of event listener vector in case of hang-crash to see
      // if http://crbug.com/1420890 is induced by event listener count runaway.
      // Only do this when we have a non-trivial number of listeners already.
      static constexpr wtf_size_t kMinNumberOfListenersToReport = 8;
      if (entry.second->size() < kMinNumberOfListenersToReport) {
        return AddListenerToVector(entry.second.Get(), listener, options,
                                   registered_listener);
      }
      SCOPED_CRASH_KEY_NUMBER("events", "listener_count_log2",
                              base::bits::Log2Floor(entry.second->size()));
      return AddListenerToVector(entry.second.Get(), listener, options,
                                 registered_listener);
    }
  }

  entries_.push_back(
      std::make_pair(event_type, MakeGarbageCollected<EventListenerVector>()));
  return AddListenerToVector(entries_.back().second.Get(), listener, options,
                             registered_listener);
}

static bool RemoveListenerFromVector(
    EventListenerVector* listener_vector,
    const EventListener* listener,
    const EventListenerOptions* options,
    RegisteredEventListener** registered_listener) {
  EventListenerVector::iterator end = listener_vector->end();
  for (EventListenerVector::iterator iter = listener_vector->begin();
       iter != end; ++iter) {
    if ((*iter)->Matches(listener, options)) {
      (*iter)->SetRemoved();
      *registered_listener = *iter;
      listener_vector->erase(iter);
      return true;
    }
  }
  return false;
}

bool EventListenerMap::Remove(const AtomicString& event_type,
                              const EventListener* listener,
                              const EventListenerOptions* options,
                              RegisteredEventListener** registered_listener) {
  for (unsigned i = 0; i < entries_.size(); ++i) {
    if (entries_[i].first == event_type) {
      bool was_removed = RemoveListenerFromVector(
          entries_[i].second.Get(), listener, options, registered_listener);
      if (entries_[i].second->empty())
        entries_.EraseAt(i);
      return was_removed;
    }
  }

  return false;
}

EventListenerVector* EventListenerMap::Find(const AtomicString& event_type) {
  for (const auto& entry : entries_) {
    if (entry.first == event_type)
      return entry.second.Get();
  }

  return nullptr;
}

static void CopyListenersNotCreatedFromMarkupToTarget(
    const AtomicString& event_type,
    EventListenerVector* listener_vector,
    EventTarget* target) {
  for (auto& event_listener : *listener_vector) {
    if (event_listener->Callback()->IsEventHandlerForContentAttribute()) {
      continue;
    }
    AddEventListenerOptionsResolved* options = event_listener->Options();
    target->addEventListener(event_type, event_listener->Callback(), options);
  }
}

void EventListenerMap::CopyEventListenersNotCreatedFromMarkupToTarget(
    EventTarget* target) {
  for (const auto& event_listener : entries_) {
    CopyListenersNotCreatedFromMarkupToTarget(
        event_listener.first, event_listener.second.Get(), target);
  }
}

void EventListenerMap::Trace(Visitor* visitor) const {
  visitor->Trace(entries_);
}

}  // namespace blink
