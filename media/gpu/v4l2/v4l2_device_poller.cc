// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_device_poller.h"

#include <string>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  StopPolling();
}

bool V4L2DevicePoller::StartPolling(EventCallback event_callback,
                                    base::RepeatingClosure error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (IsPolling())
    return true;

  client_task_runner_ = base::SequencedTaskRunnerHandle::Get();
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

  SchedulePoll();

  return true;
}

bool V4L2DevicePoller::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!IsPolling())
    return true;

  stop_polling_.store(true);

  trigger_poll_.Signal();

  if (!device_->SetDevicePollInterrupt()) {
    VLOGF(1) << "Failed to interrupt device poll.";
    return false;
  }

  DVLOGF(3) << "Stop device poll thread";
  poll_thread_.Stop();

  if (!device_->ClearDevicePollInterrupt()) {
    VLOGF(1) << "Failed to clear interrupting device poll.";
    return false;
  }

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

  trigger_poll_.Signal();
}

void V4L2DevicePoller::DevicePollTask() {
  DCHECK(poll_thread_.task_runner()->RunsTasksInCurrentSequence());

  while (true) {
    trigger_poll_.Wait();

    if (stop_polling_)
      break;

    bool event_pending = false;
    if (!device_->Poll(true, &event_pending)) {
      client_task_runner_->PostTask(FROM_HERE, error_callback_);
      return;
    }

    client_task_runner_->PostTask(FROM_HERE,
                                  base::Bind(event_callback_, event_pending));
  }
}

}  // namespace media
