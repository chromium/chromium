// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>

#include "nacl_io/error.h"
#include "nacl_io/event_listener.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/osstat.h"
#include "nacl_io/ostime.h"
#include "nacl_io/osunistd.h"

#include "sdk_util/auto_lock.h"

#if defined(WIN32)

#define USECS_FROM_WIN_TO_TO_UNIX_EPOCH 11644473600000LL
static uint64_t usec_since_epoch() {
  FILETIME ft;
  ULARGE_INTEGER ularge;
  GetSystemTimeAsFileTime(&ft);

  ularge.LowPart = ft.dwLowDateTime;
  ularge.HighPart = ft.dwHighDateTime;

  // Truncate to usec resolution.
  return ularge.QuadPart / 10;
}

#else

static uint64_t usec_since_epoch() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_usec + (tv.tv_sec * 1000000);
}

#endif

namespace nacl_io {

EventListener::EventListener() {
  pthread_cond_init(&signal_cond_, NULL);
}

EventListener::~EventListener() {
  pthread_cond_destroy(&signal_cond_);
}

static void AbsoluteFromDeltaMS(struct timespec* timeout, int ms_timeout) {
  if (ms_timeout >= 0) {
    uint64_t usec = usec_since_epoch();
    usec += ((int64_t)ms_timeout * 1000);

    timeout->tv_nsec = (usec % 1000000) * 1000;
    timeout->tv_sec = (usec / 1000000);
  } else {
    timeout->tv_sec = 0;
    timeout->tv_nsec = 0;
  }
}

EventListenerLock::EventListenerLock(EventEmitter* emitter)
    : EventListener(),
      emitter_(emitter),
      lock_(new sdk_util::AutoLock(emitter->GetLock())) {}

EventListenerLock::~EventListenerLock() {
  delete lock_;
}

void EventListenerLock::ReceiveEvents(EventEmitter* emitter, uint32_t events) {
  // We are using the emitter's mutex, which is already locked.
  pthread_cond_signal(&signal_cond_);
}

Error EventListenerLock::WaitOnEvent(uint32_t events, int ms_timeout) {
  struct timespec timeout;
  AbsoluteFromDeltaMS(&timeout, ms_timeout);

  emitter_->RegisterListener_Locked(this, events);
  while ((emitter_->GetEventStatus_Locked() & events) == 0) {
    int return_code;
    if (ms_timeout >= 0) {
      return_code = pthread_cond_timedwait(&signal_cond_,
                                           emitter_->GetLock().mutex(),
                                           &timeout);
    } else {
      return_code = pthread_cond_wait(&signal_cond_,
                                      emitter_->GetLock().mutex());
    }

    if (emitter_->GetEventStatus_Locked() & POLLERR)
      return_code = EINTR;

    // Return the failure, unlocked
    if (return_code != 0) {
      emitter_->UnregisterListener_Locked(this);
      return Error(return_code);
    }
  }

  emitter_->UnregisterListener_Locked(this);
  return 0;
}

void EventListenerPoll::ReceiveEvents(EventEmitter* emitter, uint32_t events) {
  AUTO_LOCK(signal_lock_);
  emitters_[emitter]->events |= events;
  signaled_++;
  pthread_cond_signal(&signal_cond_);
}

Error EventListenerPoll::WaitOnAny(EventRequest* requests,
                                   size_t cnt,
                                   int ms_timeout) {
  signaled_ = 0;

  // Build a map of request emitters to request data before
  // emitters can access them.
  for (size_t index = 0; index < cnt; index++) {
    EventRequest* request = requests + index;
    emitters_[request->emitter.get()] = request;
    request->events = 0;
  }

  // Emitters can now accessed the unlocked set, since each emitter is
  // responsible for it's own request.
  for (size_t index = 0; index < cnt; index++) {
    EventRequest* request = requests + index;
    request->emitter->RegisterListener(this, request->filter);
    uint32_t events = request->emitter->GetEventStatus() & request->filter;

    if (events) {
      AUTO_LOCK(signal_lock_);
      request->events |= events;
      signaled_++;
    }
  }

  struct timespec timeout;
  AbsoluteFromDeltaMS(&timeout, ms_timeout);
  int return_code = 0;

  {
    AUTO_LOCK(signal_lock_)
    while (0 == signaled_) {
      if (ms_timeout >= 0) {
        return_code = pthread_cond_timedwait(&signal_cond_,
                                              signal_lock_.mutex(),
                                              &timeout);
      } else {
        return_code = pthread_cond_wait(&signal_cond_,
                                        signal_lock_.mutex());
      }

      if (return_code != 0)
        signaled_++;
    }
  }

  // Unregister first to prevent emitters from modifying the set any further
  for (size_t index = 0; index < cnt; index++) {
    EventRequest* request = requests + index;
    request->emitter->UnregisterListener(this);

    if (request->events & POLLERR)
      return_code = EINTR;
  }

  // We can now release the map.
  emitters_.clear();

  return Error(return_code);
}

}  // namespace nacl_io
