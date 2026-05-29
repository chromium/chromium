// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/race_condition_scheduler.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/tests/testable_multiplex_router.h"

namespace mojo::test {

RaceConditionScheduler::RaceConditionScheduler()
    : router_thread_(std::make_unique<base::Thread>("Router Thread")),
      racing_thread_(std::make_unique<base::Thread>("RacingThread")),
      reset_event_waiter_(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED),
      destruction_pause_waiter_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  // The router thread needs an IO message pump because Connector's
  // SimpleWatcher uses Mojo traps that require IO support.
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  router_thread_->StartWithOptions(std::move(options));

  // The racing thread only posts tasks and releases refs — no IO needed.
  racing_thread_->Start();

  // Create a pipe but only use one end — the other end is intentionally
  // dropped, which will eventually trigger a pipe error on the router.
  MessagePipe pipe;

  router_ = TestableMultiplexRouter::CreateAndStartReceiving(
      std::move(pipe.handle0), internal::MultiplexRouter::MULTI_INTERFACE,
      false, router_thread_->task_runner());

  // Grab a weak pointer to the Connector so we can safely invoke
  // OnWatcherHandleReady even if the router (and its Connector) have been
  // destroyed by the time our posted task runs.
  connector_weak_ptr_ =
      router_->GetConnectorForTesting().GetWeakPtrForTesting();  // IN-TEST
}

RaceConditionScheduler::~RaceConditionScheduler() {
  racing_thread_.reset();
  router_thread_.reset();
}

void RaceConditionScheduler::CallOnWatcherHandleReady(
    base::OnceClosure callback,
    MojoResult result) NO_THREAD_SAFETY_ANALYSIS {
  // Run the callback first to let the test observe the sequencing (e.g.,
  // quit a RunLoop to know we've reached this point).
  std::move(callback).Run();

  // Simulate a pipe event via the Connector. Using a WeakPtr because the
  // Connector may have been destroyed if the router was deleted first.
  if (connector_weak_ptr_) {
    connector_weak_ptr_->OnWatcherHandleReady("MyCoolMojoInterface", result);
  }
}

void RaceConditionScheduler::ScheduleOnWatcherHandleReady(
    base::OnceClosure callback,
    MojoResult result) {
  router_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RaceConditionScheduler::CallOnWatcherHandleReady,
                     base::Unretained(this), std::move(callback), result));
}

void RaceConditionScheduler::ResetRouter() {
  // Signal that we're in the destructor thread and about to
  // release
  reset_event_waiter_.Signal();

  // Release the router, causing its destructor to run on this
  // thread if this was the last reference.
  router_.reset();
}

void RaceConditionScheduler::ScheduleDeathAndWait(base::OnceClosure callback) {
  router_->AddDestructionWait(&destruction_pause_waiter_, std::move(callback));

  // Post a task to the racing thread to release the router reference.
  // This will cause the destructor to start running on the racing thread.
  racing_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RaceConditionScheduler::ResetRouter,
                                base::Unretained(this)));

  reset_event_waiter_.Wait();
}

void RaceConditionScheduler::ScheduleNotifyPeerClosureAndDeath(
    base::OnceClosure error_callback,
    base::OnceClosure death_callback,
    InterfaceId id) {
  router_->AddDestructionWait(&destruction_pause_waiter_,
                              std::move(death_callback));

  // Post to the racing thread: call NotifyLocalEndpointOfPeerClosure
  // off-sequence, then release the last ref to trigger destruction.
  racing_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](RaceConditionScheduler* self, InterfaceId id,
                        base::OnceClosure callback) {
                       // Call off-sequence while we still hold a ref.
                       self->router_->NotifyLocalEndpointOfPeerClosure(id);
                       std::move(callback).Run();
                       // Release the last ref, triggering
                       // DeleteOnCorrectSequence.
                       self->reset_event_waiter_.Signal();
                       self->router_.reset();
                     },
                     base::Unretained(this), id, std::move(error_callback)));

  reset_event_waiter_.Wait();
}

void RaceConditionScheduler::ScheduleRaiseErrorAndDeath(
    base::OnceClosure error_callback,
    base::OnceClosure death_callback) {
  router_->AddDestructionWait(&destruction_pause_waiter_,
                              std::move(death_callback));

  // Post to the racing thread: call RaiseError off-sequence, then release
  // the last ref to trigger destruction.
  racing_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](RaceConditionScheduler* self, base::OnceClosure callback) {
            // Call off-sequence while we still hold a ref.
            self->router_->RaiseError();
            std::move(callback).Run();
            // Release the last ref, triggering DeleteOnCorrectSequence.
            self->reset_event_waiter_.Signal();
            self->router_.reset();
          },
          base::Unretained(this), std::move(error_callback)));

  reset_event_waiter_.Wait();
}

void RaceConditionScheduler::SignalDeath() {
  destruction_pause_waiter_.Signal();
}

}  // namespace mojo::test
