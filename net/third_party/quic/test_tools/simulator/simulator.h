// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_SIMULATOR_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_SIMULATOR_H_

#include <map>

#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/test_tools/simulator/actor.h"
#include "net/third_party/quic/test_tools/simulator/alarm_factory.h"

namespace quic {
namespace simulator {

// Simulator is responsible for scheduling actors in the simulation and
// providing basic utility interfaces (clock, alarms, RNG and others).
class Simulator : public QuicConnectionHelperInterface {
 public:
  Simulator();
  Simulator(const Simulator&) = delete;
  Simulator& operator=(const Simulator&) = delete;
  ~Simulator() override;

  // Register an actor with the simulator.  Returns a handle which the actor can
  // use to schedule and unschedule itself.
  void AddActor(Actor* actor);

  // Schedule the specified actor.  This method will ensure that |actor| is
  // called at |new_time| at latest.  If Schedule() is called multiple times
  // before the Actor is called, Act() is called exactly once, at the earliest
  // time requested, and the Actor has to reschedule itself manually for the
  // subsequent times if they are still necessary.
  void Schedule(Actor* actor, QuicTime new_time);

  // Remove the specified actor from the schedule.
  void Unschedule(Actor* actor);

  // Begin QuicConnectionHelperInterface implementation.
  const QuicClock* GetClock() const override;
  QuicRandom* GetRandomGenerator() override;
  QuicBufferAllocator* GetStreamSendBufferAllocator() override;
  // End QuicConnectionHelperInterface implementation.

  QuicAlarmFactory* GetAlarmFactory();

  inline void set_random_generator(QuicRandom* random) {
    random_generator_ = random;
  }

  inline bool enable_random_delays() const { return enable_random_delays_; }

  // Run the simulation until either no actors are scheduled or
  // |termination_predicate| returns true.  Returns true if terminated due to
  // predicate, and false otherwise.
  template <class TerminationPredicate>
  bool RunUntil(TerminationPredicate termination_predicate);

  // Same as RunUntil, except this function also accepts a |deadline|, and will
  // return false if the deadline is exceeded.
  template <class TerminationPredicate>
  bool RunUntilOrTimeout(TerminationPredicate termination_predicate,
                         QuicTime::Delta deadline);

  // Runs the simulation for exactly the specified |time_span|.
  void RunFor(QuicTime::Delta time_span);

 private:
  class Clock : public QuicClock {
   public:
    // Do not start at zero as certain code can treat zero as an invalid
    // timestamp.
    const QuicTime kStartTime =
        QuicTime::Zero() + QuicTime::Delta::FromMicroseconds(1);

    Clock();

    QuicTime ApproximateNow() const override;
    QuicTime Now() const override;
    QuicWallTime WallNow() const override;

    QuicTime now_;
  };

  // The delegate used for RunFor().
  class RunForDelegate : public QuicAlarm::Delegate {
   public:
    explicit RunForDelegate(bool* run_for_should_stop);
    void OnAlarm() override;

   private:
    // Pointer to |run_for_should_stop_| in the parent simulator.
    bool* run_for_should_stop_;
  };

  // Finds the next scheduled actor, advances time to the schedule time and
  // notifies the actor.
  void HandleNextScheduledActor();

  Clock clock_;
  QuicRandom* random_generator_;
  SimpleBufferAllocator buffer_allocator_;
  AlarmFactory alarm_factory_;

  // Alarm for RunFor() method.
  std::unique_ptr<QuicAlarm> run_for_alarm_;
  // Flag used to stop simulations ran via RunFor().
  bool run_for_should_stop_;

  // Indicates whether the simulator should add random delays on the links in
  // order to avoid synchronization issues.
  bool enable_random_delays_;

  // Schedule of when the actors will be executed via an Act() call.  The
  // schedule is subject to the following invariants:
  // - An actor cannot be scheduled for a later time than it's currently in the
  //   schedule.
  // - An actor is removed from schedule either immediately before Act() is
  //   called or by explicitly calling Unschedule().
  // - Each Actor appears in the map at most once.
  std::multimap<QuicTime, Actor*> schedule_;
  // For each actor, maintain the time it is scheduled at.  The value for
  // unscheduled actors is QuicTime::Infinite().
  QuicUnorderedMap<Actor*, QuicTime> scheduled_times_;
  QuicUnorderedSet<QuicString> actor_names_;
};

template <class TerminationPredicate>
bool Simulator::RunUntil(TerminationPredicate termination_predicate) {
  bool predicate_value = false;
  while (true) {
    predicate_value = termination_predicate();
    if (predicate_value || schedule_.empty()) {
      break;
    }
    HandleNextScheduledActor();
  }
  return predicate_value;
}

template <class TerminationPredicate>
bool Simulator::RunUntilOrTimeout(TerminationPredicate termination_predicate,
                                  QuicTime::Delta timeout) {
  QuicTime end_time = clock_.Now() + timeout;
  bool return_value = RunUntil([end_time, &termination_predicate, this]() {
    return termination_predicate() || clock_.Now() >= end_time;
  });

  if (clock_.Now() >= end_time) {
    return false;
  }
  return return_value;
}

}  // namespace simulator
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_SIMULATOR_H_
