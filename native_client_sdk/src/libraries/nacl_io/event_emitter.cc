// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <poll.h>

#include "nacl_io/event_emitter.h"
#include "nacl_io/event_listener.h"
#include "nacl_io/fifo_interface.h"

#include "sdk_util/auto_lock.h"

namespace nacl_io {

bool operator<(const ScopedEventEmitter& src_a,
               const ScopedEventEmitter& src_b) {
  return src_a.get() < src_b.get();
}

EventEmitter::EventEmitter() : event_status_(0) {
}

void EventEmitter::RegisterListener(EventListener* listener, uint32_t events) {
  AUTO_LOCK(GetLock());
  RegisterListener_Locked(listener, events);
}

void EventEmitter::UnregisterListener(EventListener* listener) {
  AUTO_LOCK(GetLock());
  UnregisterListener_Locked(listener);
}

void EventEmitter::RegisterListener_Locked(EventListener* listener,
                                           uint32_t events) {
  assert(listeners_.count(listener) == 0);
  listeners_[listener] = events;
}

void EventEmitter::UnregisterListener_Locked(EventListener* listener) {
  assert(listeners_.count(listener) == 1);
  listeners_.erase(listener);
}

void EventEmitter::ClearEvents_Locked(uint32_t event_bits) {
  event_status_ &= ~event_bits;
}

void EventEmitter::RaiseEvents_Locked(uint32_t event_bits) {
  event_status_ |= event_bits;

  EventListenerMap_t::iterator it;
  for (it = listeners_.begin(); it != listeners_.end(); it++) {
    uint32_t bits = it->second & event_bits;
    if (0 != bits)
      it->first->ReceiveEvents(this, bits);
  }
}

}  // namespace nacl_io
