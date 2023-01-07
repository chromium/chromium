// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef LIBRARIES_NACL_IO_EVENT_EMITTER_H_
#define LIBRARIES_NACL_IO_EVENT_EMITTER_H_

#include <stdint.h>

#include <map>
#include <set>

#include "nacl_io/error.h"

#include "sdk_util/auto_lock.h"
#include "sdk_util/macros.h"
#include "sdk_util/ref_object.h"
#include "sdk_util/scoped_ref.h"
#include "sdk_util/simple_lock.h"

namespace nacl_io {

class EventEmitter;
class EventListener;

typedef sdk_util::ScopedRef<EventEmitter> ScopedEventEmitter;
typedef std::map<EventListener*, uint32_t> EventListenerMap_t;

bool operator<(const ScopedEventEmitter& src_a,
               const ScopedEventEmitter& src_b);

// EventEmitter
//
// The EventEmitter class provides notification of events to EventListeners
// by registering EventInfo objects and signaling the EventListener
// whenever their state is changed.
//
// See "Kernel Events" in event_listener.h for additional information.

class EventEmitter : public sdk_util::RefObject {
 public:
  EventEmitter();

  EventEmitter(const EventEmitter&) = delete;
  EventEmitter& operator=(const EventEmitter&) = delete;

  // This returns a snapshot, to ensure the status doesn't change from
  // fetch to use, hold the lock and call GetEventStatus_Locked.
  uint32_t GetEventStatus() {
    AUTO_LOCK(GetLock());
    return GetEventStatus_Locked();
  }

  uint32_t GetEventStatus_Locked() { return event_status_; }

  virtual sdk_util::SimpleLock& GetLock() { return emitter_lock_; }

  // Updates the specified bits in the event status, and signals any
  // listeners waiting on those bits.
  void RaiseEvents_Locked(uint32_t events);

  // Clears the specified bits in the event status.
  void ClearEvents_Locked(uint32_t events);

  // Register or unregister an EventInfo.  The lock of the EventListener
  // associated with this EventInfo must be held prior to calling these
  // functions.  These functions are private to ensure they are called by the
  // EventListener.
  void RegisterListener(EventListener* listener, uint32_t events);
  void UnregisterListener(EventListener* listener);
  void RegisterListener_Locked(EventListener* listener, uint32_t events);
  void UnregisterListener_Locked(EventListener* listener);

 private:
  uint32_t event_status_;
  sdk_util::SimpleLock emitter_lock_;
  EventListenerMap_t listeners_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_EVENT_EMITTER_H_
