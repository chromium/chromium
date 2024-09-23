// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_device_poller.h"

#include <string>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

V4L2DevicePoller::V4L2DevicePoller(V4L2Device* const device,
                                   const std::string& thread_name)
    : device_(device),
      poll_thread_(std::move(thread_name)),
      trigger_poll_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
      stop_polling_(false) {
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
}

V4L2DevicePoller::~V4L2DevicePoller() {
  // It's possible the V4L2 device poller gets destroyed on a different thread
  // than expected if e.g. destroying a decoder immediately after creation. The
  // check here is not thread-safe, but using a lock or atomic state doesn't
  // make sense as destruction is never thread-safe.
  if (poll_thread_.IsRunning()) {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    StopPolling();
  }
}

bool V4L2DevicePoller::StartPolling(EventCallback event_callback,
                                    base::RepeatingClosure error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (IsPolling())
    return true;

  DVLOGF(4) << "Starting polling";

  client_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  error_callback_ = error_callback;

  if (!poll_thread_.Start()) {
    VLOGF(1) << "Failed to start device poll thread";
    return false;
  }

  event_callback_ = std::move(event_callback);

  stop_polling_.store(false);
  poll_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V4L2DevicePoller::DevicePollTask,
                                base::Unretained(this)));

  DVLOGF(3) << "Polling thread started";

  SchedulePoll();

  return true;
}

bool V4L2DevicePoller::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!IsPolling())
    return true;

  DVLOGF(4) << "Stopping polling";

  stop_polling_.store(true);

  trigger_poll_.Signal();

  if (!device_->SetDevicePollInterrupt()) {
    VLOGF(1) << "Failed to interrupt device poll.";
    return false;
  }

  DVLOGF(3) << "Stop device poll thread";
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    poll_thread_.Stop();
  }

  if (!device_->ClearDevicePollInterrupt()) {
    VLOGF(1) << "Failed to clear interrupting device poll.";
    return false;
  }

  DVLOGF(4) << "Polling thread stopped";

  return true;
}

bool V4L2DevicePoller::IsPolling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return poll_thread_.IsRunning();
}

void V4L2DevicePoller::SchedulePoll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // A call to DevicePollTask() will be posted when we actually start polling.
  if (!IsPolling())
    return;

  DVLOGF(4) << "Scheduling poll";

  trigger_poll_.Signal();
}

void V4L2DevicePoller::DevicePollTask() {
  DCHECK(poll_thread_.task_runner()->RunsTasksInCurrentSequence());

  while (true) {
    DVLOGF(4) << "Waiting for poll to be scheduled.";
    trigger_poll_.Wait();

    if (stop_polling_) {
      DVLOGF(4) << "Poll stopped, exiting.";
      break;
    }

    bool event_pending = false;
    DVLOGF(4) << "Polling device.";
    if (!device_->Poll(true, &event_pending)) {
      VLOGF(1) << "An error occurred while polling, calling error callback";
      client_task_runner_->PostTask(FROM_HERE, error_callback_);
      return;
    }

    DVLOGF(4) << "Poll returned, calling event callback.";
    client_task_runner_->PostTask(
        FROM_HERE, base::BindRepeating(event_callback_, event_pending));
  }
}

}  // namespace media
