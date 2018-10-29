// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_ACTOR_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_ACTOR_H_

#include <string>

#include "net/third_party/quic/core/quic_time.h"
#include "net/third_party/quic/platform/api/quic_clock.h"

namespace quic {
namespace simulator {

class Simulator;

// Actor is the base class for all participants of the simulation which can
// schedule events to be triggered at the specified time.  Every actor has a
// name assigned to it, which can be used for debugging and addressing purposes.
//
// The Actor object is scheduled as follows:
// 1. Every Actor object appears at most once in the event queue, for one
//    specific time.
// 2. Actor is scheduled by calling Schedule() method.
// 3. If Schedule() method is called with multiple different times specified,
//    Act() method will be called at the earliest time specified.
// 4. Before Act() is called, the Actor is removed from the event queue.  Act()
//    will not be called again unless Schedule() is called.
class Actor {
 public:
  Actor(Simulator* simulator, QuicString name);
  virtual ~Actor();

  // Trigger all the events the actor can potentially handle at this point.
  // Before Act() is called, the actor is removed from the event queue, and has
  // to schedule the next call manually.
  virtual void Act() = 0;

  inline QuicString name() const { return name_; }
  inline Simulator* simulator() const { return simulator_; }

 protected:
  // Calls Schedule() on the associated simulator.
  void Schedule(QuicTime next_tick);

  // Calls Unschedule() on the associated simulator.
  void Unschedule();

  Simulator* simulator_;
  const QuicClock* clock_;
  QuicString name_;

 private:
  // Since the Actor object registers itself with a simulator using a pointer to
  // itself, do not allow it to be moved.
  Actor(Actor&&) = delete;
  Actor(const Actor&) = delete;
  Actor& operator=(const Actor&) = delete;
  Actor& operator=(Actor&&) = delete;
};

}  // namespace simulator
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_ACTOR_H_
