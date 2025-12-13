// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/log_event_dispatcher.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/not_fatal_until.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "media/cast/cast_environment.h"

namespace media::cast {

namespace {

void RunOnThread(base::SingleThreadTaskRunner& task_runner,
                 base::OnceClosure task) {
  if (task_runner.RunsTasksInCurrentSequence()) {
    std::move(task).Run();
  } else {
    task_runner.PostTask(FROM_HERE, std::move(task));
  }
}

}  // namespace

LogEventDispatcher::LogEventDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure deletion_cb)
    : task_runner_(std::move(task_runner)),
      impl_(
          std::make_unique<LogEventDispatcher::Impl>(std::move(deletion_cb))) {}

LogEventDispatcher::~LogEventDispatcher() {
  // `impl_` is destroyed on the task runner to ensure that base::Unretained is
  // safe in callbacks that use it below.
  task_runner_->DeleteSoon(FROM_HERE, std::move(impl_));
}

void LogEventDispatcher::DispatchFrameEvent(
    std::unique_ptr<FrameEvent> event) const {
  RunOnThread(*task_runner_,
              base::BindOnce(&LogEventDispatcher::Impl::DispatchFrameEvent,
                             // Here and below: Unretained is safe because impl_
                             // is destroyed on the main task runner.
                             base::Unretained(impl_.get()), std::move(event)));
}

void LogEventDispatcher::DispatchPacketEvent(
    std::unique_ptr<PacketEvent> event) const {
  RunOnThread(*task_runner_,
              base::BindOnce(&LogEventDispatcher::Impl::DispatchPacketEvent,
                             base::Unretained(impl_.get()), std::move(event)));
}

void LogEventDispatcher::DispatchBatchOfEvents(
    std::unique_ptr<std::vector<FrameEvent>> frame_events,
    std::unique_ptr<std::vector<PacketEvent>> packet_events) const {
  RunOnThread(
      *task_runner_,
      base::BindOnce(&LogEventDispatcher::Impl::DispatchBatchOfEvents,
                     base::Unretained(impl_.get()), std::move(frame_events),
                     std::move(packet_events)));
}

void LogEventDispatcher::Subscribe(RawEventSubscriber* subscriber) {
  RunOnThread(*task_runner_,
              base::BindOnce(&LogEventDispatcher::Impl::Subscribe,
                             base::Unretained(impl_.get()), subscriber));
}

void LogEventDispatcher::Unsubscribe(RawEventSubscriber* subscriber) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    impl_->Unsubscribe(subscriber);
  } else {
    // This method, once it returns, guarantees |subscriber| will not receive
    // any more events.  Therefore, when called on a thread other than the
    // `task_runner_`'s thread, block until the unsubscribe task completes.
    base::WaitableEvent done;
    CHECK(task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](Impl* impl, RawEventSubscriber* subscriber,
                          base::WaitableEvent* done) {
                         impl->Unsubscribe(subscriber);
                         done->Signal();
                       },
                       base::Unretained(impl_.get()), subscriber, &done)));
    done.Wait();
  }
}

LogEventDispatcher::Impl::Impl(base::OnceClosure deletion_cb)
    : deletion_cb_(std::move(deletion_cb)) {}

LogEventDispatcher::Impl::~Impl() {
  CHECK(subscribers_.empty());
  if (deletion_cb_) {
    std::move(deletion_cb_).Run();
  }
}

void LogEventDispatcher::Impl::DispatchFrameEvent(
    std::unique_ptr<FrameEvent> event) const {
  for (RawEventSubscriber* s : subscribers_) {
    s->OnReceiveFrameEvent(*event);
  }
}

void LogEventDispatcher::Impl::DispatchPacketEvent(
    std::unique_ptr<PacketEvent> event) const {
  for (RawEventSubscriber* s : subscribers_) {
    s->OnReceivePacketEvent(*event);
  }
}

void LogEventDispatcher::Impl::DispatchBatchOfEvents(
    std::unique_ptr<std::vector<FrameEvent>> frame_events,
    std::unique_ptr<std::vector<PacketEvent>> packet_events) const {
  for (RawEventSubscriber* s : subscribers_) {
    for (const FrameEvent& e : *frame_events) {
      s->OnReceiveFrameEvent(e);
    }
    for (const PacketEvent& e : *packet_events) {
      s->OnReceivePacketEvent(e);
    }
  }
}

void LogEventDispatcher::Impl::Subscribe(RawEventSubscriber* subscriber) {
  CHECK(!base::Contains(subscribers_, subscriber));
  subscribers_.push_back(subscriber);
}

void LogEventDispatcher::Impl::Unsubscribe(RawEventSubscriber* subscriber) {
  const auto it = std::ranges::find(subscribers_, subscriber);
  CHECK(it != subscribers_.end());
  subscribers_.erase(it);
}

}  // namespace media::cast
