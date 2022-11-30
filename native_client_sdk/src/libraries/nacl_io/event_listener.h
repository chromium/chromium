// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_EVENT_LISTENER_H_
#define LIBRARIES_NACL_IO_EVENT_LISTENER_H_

#include <pthread.h>

#include <map>
#include <set>
#include <vector>

#include "nacl_io/error.h"
#include "nacl_io/event_emitter.h"

#include "sdk_util/auto_lock.h"
#include "sdk_util/macros.h"
#include "sdk_util/scoped_ref.h"

// Kernel Events
//
// Certain file objects such as pipes or sockets can become signaled when
// read or write buffers become available, or when the connection is torn
// down.  EventListener provides a mechanism for a thread to wait on
// specific events from these objects which are derived from EventEmitters.
//
// Calling RegisterListener_Locked on an event emitter, will cause all
// Listeners matching the event mask are signaled so they may try to make
// progress.  In the case of "select" or "poll", multiple threads could be
// notified that one or more emitters are signaled and allowed to make
// progress.  In the case of "read" or "write", only one thread at a time
// should make progress, to ensure that if one thread consumes the signal,
// the other can correctly timeout.
//
// Events Listeners requirements:
//   1- Must reference counting Emitters to ensure they are not destroyed
//      while waiting for a signal.
//   2- Must unregister themselves from all emitters prior to being destoryed.
//   3- Must never be shared between threads since interals may not be locked
//      to prevent dead-locks with emitter signals.
//   4- Must never lock themselves before locking an emitter to prevent
//      dead-locks
//
// There are two types of listeners, EventListenerSingle and EventListenerGroup
// For Single listeners, all listeners are unblocked by the Emitter, but
// they individually take the emitters lock and test against the current
// status to ensure another listener didn't consume the signal.
//
//  Locking:
//    EventEmitter::<Backgroun IO>
//      *LOCK* EventEmitter::emitter_lock_
//        EventEmitter::RaiseEvent_Locked
//          EventListenerSingle::ReceiveEvents
//            <no locking, using emitter's lock>
//        EventListenerGroup::ReceiveEvents
//          *LOCK*  EventListenerGroup::signal_lock_
//
//    EventListenerSingle::WaitOnLock
//      *LOCK* EventEmitter::emitter_lock_
//
//    EventListenerGroup::WaitOnAny
//      *LOCK* EventListenerGroup::signal_lock_
//

namespace nacl_io {

struct EventData {
  // Bit Mask of signaled POLL events.
  uint32_t events;
  uint64_t user_data;
};

struct EventRequest {
  ScopedEventEmitter emitter;
  uint32_t filter;
  uint32_t events;
};

class EventListener;
class EventListenerGroup;
class EventListenerSingle;

typedef std::map<EventEmitter*, EventRequest*> EmitterRequestMap_t;

// EventListener
//
// The EventListener class provides an object to wait on for specific events
// from EventEmitter objects.  The EventListener becomes signalled for
// read when events are waiting, making it is also an Emitter.
class EventListener {
 public:
  EventListener();

  EventListener(const EventListener&) = delete;
  EventListener& operator=(const EventListener&) = delete;

  ~EventListener();

  // Called by EventEmitter to signal the Listener that a new event is
  // available.
  virtual void ReceiveEvents(EventEmitter* emitter, uint32_t events) = 0;

 protected:
  pthread_cond_t signal_cond_;
};

// EventListenerLock
//
// On construction, references and locks the emitter.  WaitOnEvent will
// temporarily unlock waiting for any event in |events| to become signaled.
// The functione exits with the lock taken.  The destructor will automatically
// unlock the emitter.
class EventListenerLock : public EventListener {
 public:
  explicit EventListenerLock(EventEmitter* emitter);

  EventListenerLock(const EventListenerLock&) = delete;
  EventListenerLock& operator=(const EventListenerLock&) = delete;

  ~EventListenerLock();

  // Called by EventEmitter to signal the Listener that a new event is
  // available.
  virtual void ReceiveEvents(EventEmitter* emitter, uint32_t events);

  // Called with the emitters lock held (which happens in the constructor).
  // Waits in a condvar until one of the events in |events| is raised or
  // or the timeout expired.  Returns with the emitter lock held, which
  // will be release when the destructor is called.
  //
  // On Error:
  //   ETIMEOUT if the timeout is exceeded.
  //   EINTR if the wait was interrupted.
  Error WaitOnEvent(uint32_t events, int ms_max);

 private:
  EventEmitter* emitter_;
  sdk_util::AutoLock* lock_;
};

class EventListenerPoll : public EventListener {
 public:
  EventListenerPoll() : EventListener(), signaled_(0) {}

  EventListenerPoll(const EventListenerPoll&) = delete;
  EventListenerPoll& operator=(const EventListenerPoll&) = delete;

  // Called by EventEmitter to signal the Listener that a new event is
  // available.
  virtual void ReceiveEvents(EventEmitter* emitter, uint32_t events);

  // Wait for the any requested emitter/filter pairs to emit one of the
  // events in the matching filter.  Returns 0 on success.
  //
  // On Error:
  //   ETIMEOUT if the timeout is exceeded.
  //   EINTR if the wait was interrupted.
  Error WaitOnAny(EventRequest* requests, size_t cnt, int ms_max);

 private:
  sdk_util::SimpleLock signal_lock_;
  EmitterRequestMap_t emitters_;
  size_t signaled_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_EVENT_LISTENER_H_
